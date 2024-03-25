import QtQuick

import MuseScore.Ui
import MuseScore.UiComponents
import Audacity.ProjectScene

Item {
    id: root

    signal activeFocusRequested()

    width: view.width
    height: view.height

    Component.onCompleted: {
        toolbarModel.load()
    }

    property NavigationPanel navigationPanel: NavigationPanel {
        name: "ProjectSomeToolBar"
        enabled: root.enabled && root.visible
        accessible.name: qsTrc("projectscene", "Some toolbar")
        onActiveChanged: function(active) {
            if (active) {
                root.activeFocusRequested()
                root.forceActiveFocus()
            }
        }
    }

    PlayToolBarModel {
        id: toolbarModel
    }

    ListView {
        id: view

        width: contentWidth
        height: 40

        orientation: Qt.Horizontal
        interactive: false
        spacing: 2

        model: toolbarModel

        delegate: FlatButton {

            anchors.top: parent.top
            anchors.bottom: parent.bottom

            property var item: Boolean(model) ? model.itemRole : null

            //text: Boolean(item) ? item.title : ""
            icon: Boolean(item) ? item.icon : IconCode.NONE
            iconFont: ui.theme.toolbarIconsFont

            toolTipTitle: Boolean(item) ? item.title : ""
            toolTipDescription: Boolean(item) ? item.description : ""
            toolTipShortcut: Boolean(item) ? item.shortcuts : ""

            enabled: Boolean(item) ? item.enabled : false

            textFont: ui.theme.largeBodyFont

            navigation.panel: root.navigationPanel
            navigation.name: toolTipTitle
            navigation.order: model.index
            accessible.name: (item.checkable ? (item.checked ? item.title + "  " + qsTrc("global", "On") :
                                                               item.title + "  " + qsTrc("global", "Off")) : item.title)

            transparent: true
            orientation: Qt.Horizontal

            onClicked: {
                toolbarModel.handleMenuItem(item.id)
            }
        }
    }
}
