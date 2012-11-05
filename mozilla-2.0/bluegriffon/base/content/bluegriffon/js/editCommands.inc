var cmdBgFindCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (EditorUtils.getCurrentEditorElement() &&
            EditorUtils.isDocumentEditable() &&
            EditorUtils.isEditingRenderedHTML());
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    var isSource = (GetCurrentViewMode() == "source");
    WysiwygShowFindBar();
    gDialog.bespinLineLabel.hidden = !isSource;
    gDialog.bespinLineTextbox.hidden = !isSource;
  }
};

var bgSpellingCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (EditorUtils.getCurrentEditorElement() &&
            EditorUtils.isDocumentEditable() &&
            EditorUtils.isEditingRenderedHTML() &&
            GetCurrentViewMode() == "wysiwyg");
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    window.cancelSendMessage = false;
    try {
      OpenAppModalWindow(window,
                         "chrome://bluegriffon/content/dialogs/spellCheck.xul",
                         "Spell Check",
                         false); 
    }
    catch(ex) {}
    window.content.focus();
  }
};

var cmdCopyHTMLCodeCommand = {
  isCommandEnabled: function(aCommand, dummy)
  {
    var isHTMLView = (EditorUtils.getCurrentEditorElement() &&
                      EditorUtils.isDocumentEditable() &&
                      EditorUtils.isEditingRenderedHTML() &&
                      GetCurrentViewMode() == "wysiwyg");
    if (isHTMLView) {
      var editor = EditorUtils.getCurrentEditor();
      var selection = editor.selection;
      if (selection.rangeCount == 1)
      {
        // We have a "normal" single-range selection
        if (!selection.isCollapsed) {
           return true;
         }
      }
    }
    return false;
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    var editor    = EditorUtils.getCurrentEditor();
    var selection = editor.selection;
    selection instanceof Components.interfaces.nsISelectionPrivate;
    var mimeType = EditorUtils.getCurrentDocument().contentType;

    var flags = 1 << 1; // OutputFormatted
    flags |= 1 << 5; // OutputWrap
    flags |= 1 << 10; // OutputLF

    var source = selection.toStringWithFormat(mimeType,
                                              flags,
                                              0);

    var clipboardSvc = Components.classes["@mozilla.org/widget/clipboard;1"]
                         .getService(Components.interfaces.nsIClipboard);
    var xferable = Components.classes["@mozilla.org/widget/transferable;1"]
                     .createInstance(Components.interfaces.nsITransferable);
    xferable.addDataFlavor("text/unicode");
    var s = Components.classes["@mozilla.org/supports-string;1"]
              .createInstance(Components.interfaces.nsISupportsString);
    s.data = source;
    xferable.setTransferData("text/unicode", s, source.length * 2);
    clipboardSvc.setData(xferable, null, Components.interfaces.nsIClipboard.kGlobalClipboard);
  }
};

