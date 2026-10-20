import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: root
    signal openSettings()

    property var selectedIds: []
    property int anchorIndex: -1

    readonly property bool searching: vault.searchQuery.trim().length > 0
    readonly property bool hasSelection: selectedIds.length > 0
    readonly property bool singleSelection: selectedIds.length === 1
    readonly property bool allSelected: list.count > 0 && selectedIds.length === list.count
    readonly property bool overlayOpen: mkdirDialog.visible || renameDialog.visible
                                        || deleteDialog.visible
                                        || itemMenu.visible || blankMenu.visible
    readonly property bool listShortcutsOn: visible && !overlayOpen && !vault.busy && !searchField.activeFocus

    readonly property real checkW: 30
    readonly property real colsW: Math.max(320, list.width - 24 - checkW)
    readonly property real nameW: colsW * 0.56
    readonly property real dateW: colsW * 0.26
    readonly property real sizeW: colsW - nameW - dateW

    function plural(n, one, many) {
        return n + " " + (n === 1 ? one : many)
    }

    function visibleIds() {
        return Array.from(vault.currentVisibleIds())
    }

    function clearSelection() {
        selectedIds = []
        anchorIndex = -1
    }

    function selectOnly(id, idx) {
        selectedIds = [id]
        anchorIndex = idx
    }

    function toggle(id, idx) {
        const copy = selectedIds.slice()
        const i = copy.indexOf(id)
        if (i >= 0)
            copy.splice(i, 1)
        else
            copy.push(id)
        selectedIds = copy
        anchorIndex = idx
    }

    function selectRange(idx) {
        const ids = visibleIds()
        if (anchorIndex < 0 || anchorIndex >= ids.length) {
            selectOnly(ids[idx], idx)
            return
        }
        const a = Math.min(anchorIndex, idx)
        const b = Math.max(anchorIndex, idx)
        selectedIds = ids.slice(a, b + 1)
    }

    function selectAll() {
        selectedIds = visibleIds()
    }

    function moveSelection(delta) {
        const ids = visibleIds()
        if (ids.length === 0)
            return
        let idx = anchorIndex < 0 ? (delta > 0 ? 0 : ids.length - 1) : anchorIndex + delta
        idx = Math.max(0, Math.min(ids.length - 1, idx))
        selectOnly(ids[idx], idx)
        list.positionViewAtIndex(idx, ListView.Contain)
    }

    function pruneSelection() {
        const ids = visibleIds()
        selectedIds = selectedIds.filter(id => ids.indexOf(id) !== -1)
        if (selectedIds.length === 0)
            anchorIndex = -1
    }

    function requestRename() {
        if (!singleSelection)
            return
        renameDialog.entryId = selectedIds[0]
        renameDialog.open()
    }

    function requestDelete() {
        if (!hasSelection)
            return
        if (!vault.confirmDelete) {
            performDelete()
            return
        }
        deleteDialog.summary = singleSelection
                ? qsTr("“%1” will be permanently deleted from the vault.").arg(vault.entryName(selectedIds[0]))
                : qsTr("%1 will be permanently deleted from the vault.")
                      .arg(plural(selectedIds.length, qsTr("item"), qsTr("items")))
        deleteDialog.open()
    }

    function performDelete() {
        const ids = selectedIds.slice()
        vault.deleteEntries(ids)
        toast.show(qsTr("Deleted %1.").arg(plural(ids.length, qsTr("item"), qsTr("items"))))
    }

    function toggleAll() {
        if (allSelected)
            clearSelection()
        else
            selectAll()
    }

    function openSelected() {
        if (singleSelection)
            vault.openOrEnter(selectedIds[0])
    }

    function copySelected(cut) {
        if (!hasSelection)
            return
        if (cut)
            vault.cutEntries(selectedIds)
        else
            vault.copyEntries(selectedIds)
        toast.show(cut
                   ? qsTr("%1 cut. Paste in the vault or in File Explorer.")
                         .arg(plural(selectedIds.length, qsTr("item"), qsTr("items")))
                   : qsTr("%1 copied. Paste in the vault or in File Explorer.")
                         .arg(plural(selectedIds.length, qsTr("item"), qsTr("items"))))
    }

    function enter(folderId) {
        searchField.text = ""
        vault.navigate(folderId)
        list.forceActiveFocus()
    }

    onVisibleChanged: {
        if (visible) {
            clearSelection()
            searchField.text = ""
            list.forceActiveFocus()
        }
    }

    Connections {
        target: vault.files
        function onModelReset() { root.pruneSelection() }
    }

    Connections {
        target: vault
        function onLastErrorChanged() {
            if (root.visible && vault.lastError.length > 0)
                toast.show(vault.lastError, "error")
        }
    }

    // ---------- inline components ----------

    component ToolSep: Rectangle {
        Layout.preferredWidth: 1
        Layout.preferredHeight: 22
        Layout.leftMargin: 4
        Layout.rightMargin: 4
        color: Theme.line
    }

    component ToolButton: AppButton {
        variant: "ghost"
        leftPadding: 10
        rightPadding: 10
        focusPolicy: Qt.NoFocus
    }

    component SortHeader: Item {
        id: sortHeader
        property string key
        property string label
        property int align: Text.AlignLeft
        height: parent ? parent.height : 30

        Text {
            anchors.fill: parent
            anchors.leftMargin: sortHeader.align === Text.AlignLeft ? 0 : 4
            anchors.rightMargin: 4
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: sortHeader.align
            text: sortHeader.label + (vault.sortKey === sortHeader.key
                                      ? (vault.sortAscending ? "  ▲" : "  ▼") : "")
            font.pixelSize: 12
            font.bold: true
            color: headerMouse.containsMouse || vault.sortKey === sortHeader.key ? Theme.text : Theme.muted
        }
        MouseArea {
            id: headerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: vault.sortBy(sortHeader.key)
        }
    }

    component CtxItem: MenuItem {
        id: ctxItem
        property bool danger: false
        property string hint: ""
        implicitWidth: 220
        implicitHeight: 32
        leftPadding: 14
        rightPadding: 14

        contentItem: Item {
            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: ctxItem.text
                font.pixelSize: 13
                color: !ctxItem.enabled ? "#b0b8c2" : (ctxItem.danger ? Theme.danger : Theme.text)
            }
            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: ctxItem.hint
                font.pixelSize: 12
                color: "#9aa4b1"
            }
        }
        background: Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            radius: 4
            color: ctxItem.highlighted && ctxItem.enabled ? Theme.rowHover : "transparent"
        }
    }

    component CtxSeparator: MenuSeparator {
        topPadding: 4
        bottomPadding: 4
        contentItem: Rectangle {
            implicitHeight: 1
            color: Theme.line
        }
    }

    component CtxMenu: Menu {
        topPadding: 6
        bottomPadding: 6
        background: Rectangle {
            implicitWidth: 220
            color: Theme.panel
            radius: 8
            border.color: Theme.line
        }
    }

    // ---------- dialogs ----------

    FileDialog {
        id: importFilesDialog
        title: qsTr("Import files")
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            const paths = []
            for (let i = 0; i < selectedFiles.length; ++i)
                paths.push(selectedFiles[i].toString())
            vault.importPaths(paths)
        }
    }

    FolderDialog {
        id: importFolderDialog
        title: qsTr("Import folder")
        onAccepted: vault.importPaths([selectedFolder.toString()])
    }

    FolderDialog {
        id: exportDialog
        title: qsTr("Export to folder")
        onAccepted: vault.exportEntries(root.selectedIds, selectedFolder.toString())
    }

    AppDialog {
        id: mkdirDialog
        title: qsTr("New folder")
        okText: qsTr("Create")
        okEnabled: mkdirField.text.trim().length > 0
        onOpened: {
            mkdirField.text = ""
            mkdirField.forceActiveFocus()
        }
        onAccepted: vault.mkdir(mkdirField.text.trim())
        onClosed: list.forceActiveFocus()

        AppTextField {
            id: mkdirField
            width: 360
            placeholderText: qsTr("Folder name")
            Keys.onReturnPressed: if (mkdirDialog.okEnabled) mkdirDialog.accept()
            Keys.onEnterPressed: if (mkdirDialog.okEnabled) mkdirDialog.accept()
        }
    }

    AppDialog {
        id: renameDialog
        title: qsTr("Rename")
        okText: qsTr("Rename")
        property string entryId: ""
        okEnabled: renameField.text.trim().length > 0
        onOpened: {
            const name = vault.entryName(entryId)
            renameField.text = name
            const dot = name.lastIndexOf(".")
            renameField.select(0, dot > 0 ? dot : name.length)
            renameField.forceActiveFocus()
        }
        onAccepted: vault.renameEntry(entryId, renameField.text.trim())
        onClosed: list.forceActiveFocus()

        AppTextField {
            id: renameField
            width: 360
            Keys.onReturnPressed: if (renameDialog.okEnabled) renameDialog.accept()
            Keys.onEnterPressed: if (renameDialog.okEnabled) renameDialog.accept()
        }
    }

    AppDialog {
        id: deleteDialog
        title: qsTr("Are you sure?")
        okText: qsTr("Delete")
        destructive: true
        property string summary: ""
        onOpened: dontAskAgain.checked = false
        onAccepted: {
            if (dontAskAgain.checked)
                vault.confirmDelete = false
            root.performDelete()
        }
        onClosed: list.forceActiveFocus()

        ColumnLayout {
            width: 360
            spacing: 6
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.text
                font.pixelSize: 14
                text: deleteDialog.summary
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.muted
                font.pixelSize: 12
                text: qsTr("The encrypted data is removed from disk. This can't be undone.")
            }
            AppCheckBox {
                id: dontAskAgain
                Layout.topMargin: 10
                text: qsTr("Don't ask me again")
            }
            Label {
                Layout.fillWidth: true
                visible: dontAskAgain.checked
                wrapMode: Text.WordWrap
                color: Theme.muted
                font.pixelSize: 12
                text: qsTr("You can turn this back on in Settings.")
            }
        }
    }

    // ---------- context menus ----------

    CtxMenu {
        id: itemMenu
        CtxItem { text: qsTr("Open"); hint: "Enter"; enabled: root.singleSelection; onTriggered: root.openSelected() }
        CtxItem { text: qsTr("Rename"); hint: "F2"; enabled: root.singleSelection; onTriggered: root.requestRename() }
        CtxItem { text: qsTr("Export…"); onTriggered: exportDialog.open() }
        CtxSeparator {}
        CtxItem { text: qsTr("Cut"); hint: "Ctrl+X"; onTriggered: root.copySelected(true) }
        CtxItem { text: qsTr("Copy"); hint: "Ctrl+C"; onTriggered: root.copySelected(false) }
        CtxItem { text: qsTr("Paste"); hint: "Ctrl+V"; enabled: vault.canPaste; onTriggered: vault.paste() }
        CtxSeparator {}
        CtxItem { text: qsTr("Delete"); hint: "Del"; danger: true; onTriggered: root.requestDelete() }
    }

    CtxMenu {
        id: blankMenu
        CtxItem { text: qsTr("New folder"); hint: "Ctrl+Shift+N"; onTriggered: mkdirDialog.open() }
        CtxItem { text: qsTr("Import files…"); hint: "Ctrl+O"; onTriggered: importFilesDialog.open() }
        CtxItem { text: qsTr("Import folder…"); onTriggered: importFolderDialog.open() }
        CtxSeparator {}
        CtxItem { text: qsTr("Paste"); hint: "Ctrl+V"; enabled: vault.canPaste; onTriggered: vault.paste() }
        CtxItem { text: qsTr("Select all"); hint: "Ctrl+A"; enabled: list.count > 0; onTriggered: root.selectAll() }
    }

    // ---------- shortcuts ----------

    Shortcut { sequences: [StandardKey.Delete]; enabled: root.listShortcutsOn && root.hasSelection; onActivated: root.requestDelete() }
    Shortcut { sequence: "F2"; enabled: root.listShortcutsOn && root.singleSelection; onActivated: root.requestRename() }
    Shortcut { sequences: [StandardKey.SelectAll]; enabled: root.listShortcutsOn; onActivated: root.selectAll() }
    Shortcut { sequences: [StandardKey.Copy]; enabled: root.listShortcutsOn && root.hasSelection; onActivated: root.copySelected(false) }
    Shortcut { sequences: [StandardKey.Cut]; enabled: root.listShortcutsOn && root.hasSelection; onActivated: root.copySelected(true) }
    Shortcut { sequences: [StandardKey.Paste]; enabled: root.listShortcutsOn && vault.canPaste; onActivated: vault.paste() }
    Shortcut { sequences: ["Backspace", "Alt+Up"]; enabled: root.listShortcutsOn && !vault.atRoot; onActivated: root.enter(vault.breadcrumbs[vault.breadcrumbs.length - 2].id) }
    Shortcut { sequences: ["Return", "Enter"]; enabled: root.listShortcutsOn && root.singleSelection; onActivated: root.openSelected() }
    Shortcut { sequence: "Ctrl+Shift+N"; enabled: root.listShortcutsOn; onActivated: mkdirDialog.open() }
    Shortcut { sequence: "Ctrl+O"; enabled: root.listShortcutsOn; onActivated: importFilesDialog.open() }
    Shortcut {
        sequences: [StandardKey.Find]
        enabled: root.visible && !root.overlayOpen
        onActivated: {
            searchField.forceActiveFocus()
            searchField.selectAll()
        }
    }
    Shortcut { sequence: "Ctrl+L"; enabled: root.visible && !root.overlayOpen; onActivated: vault.lock() }
    Shortcut { sequence: "Escape"; enabled: root.listShortcutsOn && root.hasSelection; onActivated: root.clearSelection() }

    // ---------- layout ----------

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header: lock, breadcrumbs, search, account
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Theme.header }
                GradientStop { position: 1; color: Theme.headerHi }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                AppButton {
                    text: qsTr("Lock")
                    variant: "header"
                    focusPolicy: Qt.NoFocus
                    implicitWidth: 72
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    ToolTip.text: qsTr("Lock vault and wipe keys (Ctrl+L)")
                    onClicked: vault.lock()
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        visible: !root.searching

                        Repeater {
                            model: vault.breadcrumbs
                            delegate: Row {
                                id: crumb
                                required property var modelData
                                required property int index
                                readonly property bool isLast: index === vault.breadcrumbs.length - 1
                                spacing: 6
                                anchors.verticalCenter: parent.verticalCenter

                                Text {
                                    visible: crumb.index > 0
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "›"
                                    color: Theme.headerMuted
                                    font.pixelSize: 18
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: crumb.modelData.name
                                    color: crumb.isLast || crumbMouse.containsMouse ? "white" : Theme.headerMuted
                                    font.pixelSize: crumb.isLast ? 18 : 15
                                    font.bold: crumb.isLast
                                    font.underline: !crumb.isLast && crumbMouse.containsMouse

                                    MouseArea {
                                        id: crumbMouse
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        enabled: !crumb.isLast
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.enter(crumb.modelData.id)
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.searching
                        text: qsTr("Search results for “%1”").arg(vault.searchQuery.trim())
                        color: "white"
                        font.pixelSize: 17
                        font.bold: true
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }

                AppTextField {
                    id: searchField
                    Layout.preferredWidth: 260
                    implicitHeight: 34
                    rightPadding: 30
                    placeholderText: qsTr("Search vault (Ctrl+F)")
                    onTextChanged: vault.searchQuery = text
                    Keys.onEscapePressed: {
                        text = ""
                        list.forceActiveFocus()
                    }
                    Keys.onDownPressed: {
                        list.forceActiveFocus()
                        root.moveSelection(1)
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        visible: searchField.text.length > 0
                        text: "×"
                        font.pixelSize: 18
                        color: clearMouse.containsMouse ? Theme.text : Theme.muted
                        MouseArea {
                            id: clearMouse
                            anchors.fill: parent
                            anchors.margins: -6
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                searchField.text = ""
                                list.forceActiveFocus()
                            }
                        }
                    }
                }

                AppButton {
                    text: qsTr("Settings")
                    variant: "header"
                    focusPolicy: Qt.NoFocus
                    enabled: !vault.busy
                    onClicked: root.openSettings()
                }
            }
        }

        // Toolbar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.panel

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.line
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 2

                ToolButton {
                    text: qsTr("↑  Up")
                    enabled: !vault.atRoot && !vault.busy && !root.searching
                    onClicked: root.enter(vault.breadcrumbs[vault.breadcrumbs.length - 2].id)
                }
                ToolSep {}
                AppButton {
                    text: qsTr("Import files")
                    variant: "primary"
                    focusPolicy: Qt.NoFocus
                    enabled: !vault.busy
                    onClicked: importFilesDialog.open()
                }
                ToolButton { text: qsTr("Import folder"); enabled: !vault.busy; onClicked: importFolderDialog.open() }
                ToolButton { text: qsTr("Export"); enabled: root.hasSelection && !vault.busy; onClicked: exportDialog.open() }
                ToolSep {}
                ToolButton { text: qsTr("New folder"); enabled: !vault.busy && !root.searching; onClicked: mkdirDialog.open() }
                ToolButton { text: qsTr("Rename"); enabled: root.singleSelection && !vault.busy; onClicked: root.requestRename() }
                ToolButton { text: qsTr("Delete"); enabled: root.hasSelection && !vault.busy; onClicked: root.requestDelete() }
                ToolSep {}
                ToolButton { text: qsTr("Cut"); enabled: root.hasSelection && !vault.busy; onClicked: root.copySelected(true) }
                ToolButton { text: qsTr("Copy"); enabled: root.hasSelection && !vault.busy; onClicked: root.copySelected(false) }
                ToolButton { text: qsTr("Paste"); enabled: vault.canPaste && !vault.busy && !root.searching; onClicked: vault.paste() }
                Item { Layout.fillWidth: true }
            }
        }

        // Column headers
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: Theme.subtle

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.line
            }

            Row {
                x: 12
                height: parent.height

                Item {
                    width: root.checkW
                    height: parent.height

                    CheckSquare {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.hasSelection
                        checked: root.allSelected
                        partial: root.hasSelection && !root.allSelected
                        hovered: headerCheckMouse.containsMouse
                    }
                    MouseArea {
                        id: headerCheckMouse
                        anchors.fill: parent
                        enabled: root.hasSelection
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.toggleAll()
                    }
                }

                SortHeader { key: "name"; label: qsTr("Name"); width: root.nameW }
                SortHeader { key: "date"; label: qsTr("Modified"); width: root.dateW }
                SortHeader { key: "size"; label: qsTr("Size"); width: root.sizeW; align: Text.AlignRight }
            }
        }

        // File list
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                anchors.fill: parent
                color: Theme.panel
            }

            ListView {
                id: list
                anchors.fill: parent
                clip: true
                focus: true
                model: vault.files
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Keys.onUpPressed: root.moveSelection(-1)
                Keys.onDownPressed: root.moveSelection(1)

                delegate: Rectangle {
                    id: row
                    required property string entryId
                    required property string name
                    required property bool isDir
                    required property string displayDate
                    required property string displaySize
                    required property int index
                    readonly property bool selected: root.selectedIds.indexOf(entryId) !== -1
                    readonly property bool hovered: rowMouse.containsMouse || checkMouse.containsMouse

                    width: ListView.view.width
                    height: 36
                    color: selected ? Theme.rowSelected : (hovered ? Theme.rowHover : "transparent")

                    Rectangle {
                        visible: row.selected
                        width: 3
                        height: parent.height
                        color: Theme.accent
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: "#f0f2f5"
                    }

                    Row {
                        x: 12
                        height: parent.height

                        Item {
                            width: root.checkW
                            height: parent.height
                            CheckSquare {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: root.hasSelection || row.hovered
                                checked: row.selected
                                hovered: checkMouse.containsMouse
                            }
                        }

                        Item {
                            width: root.nameW
                            height: parent.height
                            FileGlyph {
                                isDir: row.isDir
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                x: 26
                                width: parent.width - 34
                                anchors.verticalCenter: parent.verticalCenter
                                text: row.name
                                elide: Text.ElideMiddle
                                color: Theme.text
                                font.pixelSize: 13
                            }
                        }
                        Text {
                            width: root.dateW
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            text: row.displayDate
                            color: Theme.muted
                            font.pixelSize: 13
                        }
                        Text {
                            width: root.sizeW
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignRight
                            rightPadding: 4
                            text: row.displaySize
                            color: Theme.muted
                            font.pixelSize: 13
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressed: (mouse) => {
                            list.forceActiveFocus()
                            if (mouse.button === Qt.RightButton) {
                                if (!row.selected)
                                    root.selectOnly(row.entryId, row.index)
                                itemMenu.popup()
                                return
                            }
                            if (mouse.modifiers & Qt.ShiftModifier)
                                root.selectRange(row.index)
                            else if (mouse.modifiers & Qt.ControlModifier)
                                root.toggle(row.entryId, row.index)
                            else
                                root.selectOnly(row.entryId, row.index)
                        }
                        onDoubleClicked: (mouse) => {
                            if (mouse.button === Qt.LeftButton) {
                                if (row.isDir)
                                    root.enter(row.entryId)
                                else
                                    vault.openOrEnter(row.entryId)
                            }
                        }
                    }

                    // Checkbox hit area sits above the row so it toggles instead of selecting only.
                    MouseArea {
                        id: checkMouse
                        x: 6
                        width: root.checkW + 12
                        height: parent.height
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onPressed: list.forceActiveFocus()
                        onClicked: root.toggle(row.entryId, row.index)
                        onDoubleClicked: {}
                    }
                }
            }

            // Clicks on empty list space clear the selection.
            MouseArea {
                parent: list.contentItem
                y: list.originY
                width: list.width
                height: Math.max(list.height, list.contentHeight)
                z: -1
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onPressed: (mouse) => {
                    list.forceActiveFocus()
                    root.clearSelection()
                    if (mouse.button === Qt.RightButton)
                        blankMenu.popup()
                }
            }

            Column {
                anchors.centerIn: parent
                visible: list.count === 0
                spacing: 10

                Item {
                    width: 56
                    height: 56
                    anchors.horizontalCenter: parent.horizontalCenter
                    FileGlyph {
                        anchors.centerIn: parent
                        isDir: !root.searching
                        scale: 3
                        opacity: 0.6
                    }
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.searching ? qsTr("No matches") : qsTr("This folder is empty")
                    font.pixelSize: 15
                    font.bold: true
                    color: Theme.text
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.searching ? qsTr("Try a different name.")
                                         : qsTr("Drag files here, or use Import files.")
                    color: Theme.muted
                }
                AppButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !root.searching
                    text: qsTr("Import files")
                    variant: "primary"
                    onClicked: importFilesDialog.open()
                }
            }

            DropArea {
                id: dropArea
                anchors.fill: parent
                enabled: !vault.busy
                onEntered: (drag) => { drag.accepted = drag.hasUrls }
                onDropped: (drop) => {
                    if (!drop.hasUrls)
                        return
                    const paths = []
                    for (let i = 0; i < drop.urls.length; ++i)
                        paths.push(drop.urls[i].toString())
                    const move = drop.proposedAction === Qt.MoveAction
                    vault.importPaths(paths, move)
                    drop.accept(move ? Qt.MoveAction : Qt.CopyAction)
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 10
                visible: dropArea.containsDrag
                radius: 10
                color: "#142563eb"
                border.color: Theme.accent
                border.width: 2

                Label {
                    anchors.centerIn: parent
                    text: qsTr("Drop to encrypt into “%1”").arg(vault.currentFolderName)
                    font.pixelSize: 15
                    font.bold: true
                    color: Theme.accent
                }
            }

            Toast {
                id: toast
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 16
            }
        }

        // Status bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: Theme.header

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: "#34d399"
                }
                Label {
                    text: qsTr("Unlocked") + "  ·  " + vault.protectionText
                    color: "white"
                    font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                Label {
                    visible: root.hasSelection
                    text: qsTr("%1 selected").arg(root.selectedIds.length)
                    color: "#bfdbfe"
                    font.pixelSize: 12
                }
                Label {
                    text: root.plural(vault.itemCount, qsTr("item"), qsTr("items"))
                          + "  ·  " + vault.usedSizeText
                    color: Theme.headerMuted
                    font.pixelSize: 12
                }
            }
        }
    }

    // Busy overlay
    Rectangle {
        anchors.fill: parent
        visible: vault.busy
        color: "#40000000"
        z: 50

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
        }

        Rectangle {
            anchors.centerIn: parent
            width: 360
            height: busyColumn.implicitHeight + 40
            radius: 10
            color: Theme.panel
            border.color: Theme.line

            Column {
                id: busyColumn
                anchors.centerIn: parent
                width: parent.width - 40
                spacing: 12

                Label {
                    width: parent.width
                    text: vault.progressText.length ? vault.progressText : qsTr("Working…")
                    elide: Text.ElideMiddle
                    color: Theme.text
                    font.pixelSize: 14
                    font.bold: true
                }
                ProgressBar {
                    id: bar
                    width: parent.width
                    value: vault.progress
                    background: Rectangle {
                        implicitHeight: 6
                        radius: 3
                        color: Theme.line
                    }
                    contentItem: Item {
                        implicitHeight: 6
                        Rectangle {
                            width: bar.visualPosition * parent.width
                            height: parent.height
                            radius: 3
                            color: Theme.accent
                        }
                    }
                }
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: qsTr("Keep the app open until this finishes.")
                    color: Theme.muted
                    font.pixelSize: 12
                }
            }
        }
    }
}
