import cmd

import mobase
import json
import os

from .SLALSourceProcess import preprocess_slal_source
from .FNISBehaviorConvert import generate_behavior, process_fnis_txt

try:
    from PyQt6.QtCore import QDir, QTimer
    from PyQt6.QtGui import QIcon
    from PyQt6.QtWidgets import QMainWindow, QWidget, QMessageBox
    # Code specific to MO 2.5.0 and above
except ImportError:
    from PyQt5.QtCore import QDir, QTimer
    from PyQt5.QtGui import QIcon
    from PyQt5.QtWidgets import QMainWindow, QWidget, QMessageBox
    # Code specific to MO 2.4.4 and below


class SLALPreProcess(mobase.IPluginTool):
    _organizer: mobase.IOrganizer
    _mainWindow: QMainWindow
    _parentWidget: QWidget

    def __init__(self):
        super().__init__()

    def init(self, organizer: mobase.IOrganizer):
        self._organizer = organizer
        return True

    def name(self) -> str:
        return "SLAL PreProcess"

    def author(self) -> str:
        return "Acook1e"

    def description(self) -> str:
        return "PreProcess SLAL Animations to support SexLab NG"

    def version(self) -> mobase.VersionInfo:
        return mobase.VersionInfo(1, 0, 0, 0)

    def isActive(self) -> bool:
        return self._organizer.pluginSetting(self.name(), "enabled") is True

    def settings(self) -> list:
        return [
            mobase.PluginSetting("enabled", "enable this plugin", True)
        ]

    def displayName(self):
        return "SLAL PreProcess"

    def tooltip(self):
        return "Tooltip for SLAL PreProcess plugin."

    def icon(self):
        return QIcon()

    def setParentWidget(self, parent: QWidget):
        self._parentWidget = parent

    @staticmethod
    def _normalize_virtual_path(path: str) -> str:
        return path.replace('\\', '/').strip('/')

    def _scan_fnis_files(self):
        root = self._normalize_virtual_path("meshes/actors")
        root_lower = root.lower()
        # 跳过 OAR 和 DAR 相关目录，因为它们不包含 SLAL 的 txt 文件
        excluded_dir_names = {
            "dynamicanimationreplacer", "openanimationreplacer"}
        # 跳过 SexLab 的原版动画
        excluded_file_prefixes = {"fnis_sexlab", "fnis_zaz_animation"}
        matched_files = {}

        tree = self._organizer.virtualFileTree()

        def collector(path: str, entry: mobase.FileTreeEntry):
            entry_path = self._normalize_virtual_path(f"{path}{entry.name()}")
            entry_path_lower = entry_path.lower()

            if entry.isDir():
                if entry.name().lower() in excluded_dir_names:
                    return mobase.IFileTree.SKIP
                if root_lower.startswith(f"{entry_path_lower}/") or entry_path_lower.startswith(root_lower):
                    return mobase.IFileTree.CONTINUE
                return mobase.IFileTree.SKIP

            if entry_path_lower.startswith(f"{root_lower}/") and entry_path_lower.endswith('.txt'):
                if any(entry.name().lower().startswith(prefix) for prefix in excluded_file_prefixes):
                    return mobase.IFileTree.CONTINUE
                real_path = self._organizer.resolvePath(entry_path)
                if real_path:
                    matched_files[entry_path] = real_path
            return mobase.IFileTree.CONTINUE

        tree.walk(collector, '/')

        return matched_files

    def display(self):
        txt_paths = self._scan_fnis_files()
        overrite_path = self._organizer.overwritePath()

        scene_stages = {}
        for txt_path, real_path in txt_paths.items():
            data = process_fnis_txt(real_path)
            cmd_lines = data["cmd_lines"]
            mod_name = os.path.basename(txt_path).rsplit('.', 1)[0]
            output_path = os.path.join(overrite_path, txt_path)
            generate_behavior(cmd_lines, output_path, mod_name)
            scene_stages.update(data["anim_stages"])

        QMessageBox.information(
            self._parentWidget,
            self.displayName(),
            f"已扫描 meshes/actors，找到 {len(txt_paths)} 个 txt 文件, 共包含 {len(scene_stages)} 个动画场景。"
        )
