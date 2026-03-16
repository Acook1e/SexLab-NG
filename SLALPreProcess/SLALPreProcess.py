import mobase
import json
import os
from pathlib import Path

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

    def _scan_files(self, root_path: str, exclude_dirs: set[str], exclude_prefixes: set[str]) -> dict[str, str]:
        root = self._normalize_virtual_path(root_path)
        root_lower = root.lower()
        matched_files = {}

        tree = self._organizer.virtualFileTree()

        def collector(path: str, entry: mobase.FileTreeEntry):
            entry_path = self._normalize_virtual_path(f"{path}{entry.name()}")
            entry_path_lower = entry_path.lower()

            if entry.isDir():
                if entry.name().lower() in exclude_dirs:
                    return mobase.IFileTree.SKIP
                if root_lower.startswith(f"{entry_path_lower}/") or entry_path_lower.startswith(root_lower):
                    return mobase.IFileTree.CONTINUE
                return mobase.IFileTree.SKIP

            if entry_path_lower.startswith(f"{root_lower}/") and entry_path_lower.endswith('.txt'):
                if any(entry.name().lower().startswith(prefix) for prefix in exclude_prefixes):
                    return mobase.IFileTree.CONTINUE
                real_path = self._organizer.resolvePath(entry_path)
                if real_path:
                    matched_files[entry_path] = real_path
            return mobase.IFileTree.CONTINUE

        tree.walk(collector, '/')

        return matched_files

    def _process_slal_source_paths(self, overwrite_path: str) -> tuple[dict[str, str], set[str]]:
        slal_source_paths = self._scan_files("SLAnims/Source", set(), set())

        # 如果 Source 下没有子目录，walk 可能漏掉根目录文件，这里补一次根目录检索
        source_root = self._normalize_virtual_path("SLAnims/Source")
        try:
            direct_files = self._organizer.findFiles(source_root, "*.txt")
        except Exception:
            direct_files = []

        for file_path in direct_files:
            virtual_path = self._normalize_virtual_path(str(file_path))
            if not virtual_path.lower().startswith(source_root.lower()):
                virtual_path = self._normalize_virtual_path(
                    f"{source_root}/{virtual_path}")
            real_path = self._organizer.resolvePath(virtual_path)
            if real_path:
                slal_source_paths[virtual_path] = real_path

        scenes_output_dir = os.path.join(
            overwrite_path,
            "SKSE",
            "Plugins",
            "SexLabNG",
            "Scenes"
        )
        os.makedirs(scenes_output_dir, exist_ok=True)

        slal_dirs: set[str] = set()
        for _, real_path in slal_source_paths.items():
            data = preprocess_slal_source(real_path)

            name = data.get("anim_dir", "")
            if not name:
                continue
            slal_dirs.add(name.lower())

            output_path = os.path.join(scenes_output_dir, f"{name}.json")
            with open(output_path, 'w+', encoding='utf-8') as f:
                json.dump(data, f, indent=2)

        return slal_source_paths, slal_dirs

    def display(self):
        overwrite_path = self._organizer.overwritePath()

        slal_source_paths, slal_dirs = self._process_slal_source_paths(
            overwrite_path)

        # 跳过 OAR 和 DAR 相关目录，因为它们不包含 SLAL 的 txt 文件
        # 跳过 SexLab 的原版动画
        fnis_txt_paths = self._scan_files(
            "meshes/actors", {"dynamicanimationreplacer", "openanimationreplacer"}, {"fnis_sexlab", "fnis_zazanim", "fnis_babomotion"})

        scene_stages = {}
        for txt_path, real_path in fnis_txt_paths.items():
            mod_name = Path(real_path).parent.name
            if mod_name.lower() not in slal_dirs:
                continue
            output_path = Path(os.path.join(overwrite_path, txt_path))
            data = process_fnis_txt(real_path, output_path)
            cmd_lines = data["cmd_lines"]
            generate_behavior(cmd_lines, output_path, mod_name)
            scene_stages.update(data["anim_stages"])

        # 写入total_stages
        scenes_dir = os.path.join(
            overwrite_path,
            "SKSE",
            "Plugins",
            "SexLabNG",
            "Scenes"
        )
        for root, _, files in os.walk(scenes_dir):
            for file in files:
                if file.endswith('.json'):
                    scene_path = os.path.join(root, file)
                    with open(scene_path, 'r', encoding='utf-8') as f:
                        scene_data = json.load(f)
                    for name, data in scene_data.get("scenes", {}).items():
                        if data["event_prefix"] in scene_stages.keys():
                            data["total_stages"] = scene_stages[data["event_prefix"]]
                    with open(scene_path, 'w+', encoding='utf-8') as f:
                        json.dump(scene_data, f, indent=2)

        QMessageBox.information(
            self._parentWidget,
            self.displayName(),
            f"Pre-processing completed! Processed {len(slal_source_paths)} SLAL source files and {len(fnis_txt_paths)} FNIS txt files."
        )
