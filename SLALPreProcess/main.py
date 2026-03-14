import mobase
import json

import SLALSourceProcess

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

    def setParentWidget(self, widget: QWidget):
        self._parentWidget = widget

    def display(self):

        path = ""
        data = SLALSourceProcess.preprocess_slal_source(path)