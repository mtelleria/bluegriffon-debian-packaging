function onBespinFocus(aIframe)
{
  aIframe.focus();
}

function onBespinLineBlur(aElt)
{
  aElt.value = "";
}

function onBespinLineKeypress(aEvent, aElt)
{
  if (aEvent.keyCode == 13) {
    var line = aElt.value;
    EditorUtils.getCurrentSourceEditor().setCursor(parseInt(line) - 1, 0);
    onBespinLineBlur(aElt);
    onBespinFocus(EditorUtils.getCurrentSourceEditorElement());
  }
  if (aEvent.keyCode == 13 ||
      (aEvent.keyCode == 27 && !aEvent.which)) { // ESC key
    gDialog.bespinToolbox1.hidden = true;
    gDialog.bespinToolbox2.hidden = true;
    EditorUtils.getCurrentSourceEditorElement().lastNeedle = null;
    EditorUtils.getCurrentSourceEditorElement().focus();
  }
}

function ToggleBespinFindCaseSensitivity()
{
  var sourceIframe = EditorUtils.getCurrentSourceEditorElement(); 
  var selPoint = sourceIframe.getUserData("selPoint");
  BespinFind(sourceIframe.getUserData("findLastDirection"));
}

function BespinFind(aForward, aInitial)
{
  if (GetCurrentViewMode() == "wysiwyg") {
    if (!gDialog.bespinFindTextbox.value) {
      gDialog.bespinFindPrevious.hidden = true;
      gDialog.bespinFindNext.hidden = true;
      gDialog.bespinToolbox2.hidden = true;
      return false;
    }
    var findInst = EditorUtils.getCurrentEditorElement().webBrowserFind;
    findInst.searchString  = gDialog.bespinFindTextbox.value;
    findInst.matchCase     = gDialog.bespinFindCaseSensitive.checked;
    findInst.wrapFind      = true;
    findInst.findBackwards = !aForward;
    findInst.searchFrames  = true;
    findInst.entireWord    = false;

    var findInFrames = findInst.QueryInterface(Components.interfaces.nsIWebBrowserFindInFrames);
    findInFrames.rootSearchFrame = EditorUtils.getCurrentEditorElement().contentWindow;
    findInFrames.currentSearchFrame = findInFrames.rootSearchFrame;

    if (aInitial)
      EditorUtils.getCurrentEditor().beginningOfDocument();

    InContextHelper.enable(false);
    var result = findInst.findNext();

    gDialog.bespinFindTextbox.focus();
    if (result) {
      gDialog.bespinFindCaseSensitive.hidden = false;
      gDialog.bespinFindPrevious.hidden = false;
      gDialog.bespinFindNext.hidden = false;
      gDialog.bespinFindTextbox.className = "";
      gDialog.bespinToolbox2.hidden = false;
      return true;
    }
    gDialog.bespinFindPrevious.hidden = true;
    gDialog.bespinFindNext.hidden = true;
    gDialog.bespinFindTextbox.className = "notfound";
    gDialog.bespinToolbox2.hidden = true;
    return false;
  }
  else {
    var sourceIframe = EditorUtils.getCurrentSourceEditorElement();
    var sourceEditor = sourceIframe.contentWindow.gEditor;
    sourceIframe.setUserData("findLastDirection", aForward, null);
    var query = gDialog.bespinFindTextbox.value;
    var isCaseSensitive = gDialog.bespinFindCaseSensitive.checked;
    var found = false;
    if (aInitial || !sourceEditor.lastNeedle) {
      var selection = sourceEditor.getCursor(true);
      sourceEditor.lastNeedle = sourceEditor.getSearchCursor(query, { line: selection.line, ch: selection.ch }, isCaseSensitive);
      found = sourceEditor.lastNeedle.findNext();
    }
    else {
      if (aForward) {
        found = sourceEditor.lastNeedle.findNext();
      }
      else {
        found = sourceEditor.lastNeedle.findPrevious();
      }
    }

    if (!found) { // maybe we hit the document's limits
      if (aForward) {
        sourceEditor.lastNeedle = sourceEditor.getSearchCursor(query, { line: 0, ch: 0 }, isCaseSensitive);
        found = sourceEditor.lastNeedle.findNext();
      }
      else {
        var line = sourceEditor.lineCount() - 1;
        var lineProse = sourceEditor.getLine(line);
        sourceEditor.lastNeedle = sourceEditor.getSearchCursor(query, { line: line, ch: lineProse.length -1 }, isCaseSensitive);
        found = sourceEditor.lastNeedle.findPrevious();
      }
    }

    if (!found) {
      //gDialog.bespinFindCaseSensitive.hidden = true;
      gDialog.bespinFindPrevious.hidden = true;
      gDialog.bespinFindNext.hidden = true;
      gDialog.bespinFindTextbox.className = "notfound";
      gDialog.bespinToolbox2.hidden = true;
      return false;
    }

    sourceEditor.setSelection(sourceEditor.lastNeedle.from(), sourceEditor.lastNeedle.to());
    gDialog.bespinFindCaseSensitive.hidden = false;
    gDialog.bespinFindPrevious.hidden = false;
    gDialog.bespinFindNext.hidden = false;
    gDialog.bespinFindTextbox.className = "";
    gDialog.bespinToolbox2.hidden = false;
    return true;
  }
}

function onBespinFindClear(aEvent, aElt)
{
  if (!aElt.value) {
    aElt.className = "";
    gDialog.bespinFindCaseSensitive.hidden = true;
    gDialog.bespinFindPrevious.hidden = true;
    gDialog.bespinFindNext.hidden = true;
    gDialog.bespinToolbox2.hidden = true;
  }
}

function onBespinFindKeypress(aEvent)
{
  if (aEvent.keyCode == 27 && !aEvent.which) { // ESC key
    gDialog.bespinToolbox1.hidden = true;
    gDialog.bespinToolbox2.hidden = true;
    if (GetCurrentViewMode() == "source")
      EditorUtils.getCurrentSourceEditorElement().focus();
    else {
      window.content.focus();
      InContextHelper.showInContextPanel(InContextHelper.mElement);
    }
  }
}

function BespinKeyPressCallback(aEvent)
{
#ifdef XP_MACOSX
  if (aEvent.metaKey &&
      !aEvent.ctrlKey &&
      !aEvent.altKey) {
#else
  if (!aEvent.metaKey &&
      aEvent.ctrlKey &&
      !aEvent.altKey) {
#endif
    switch (aEvent.which) {
      case 102: // meta-f
      case 114: // meta-r
        aEvent.preventDefault();
        WysiwygShowFindBar();
        break;
      case 108: // meta-l
        aEvent.preventDefault();
        gDialog.bespinToolbox1.hidden = false;
        gDialog.bespinLineTextbox.focus();
        break;
      /*case 99: // meta-c XXX Workaround for Copy horked in Bespin0.9+Gecko2
      case 120: // meta-x XXX
        {
          aEvent.preventDefault();
          var sourceEditor = EditorUtils.getCurrentSourceEditor();
          var selection = sourceEditor.selectedText;
          var clipboardSvc = Components.classes["@mozilla.org/widget/clipboard;1"].
                             getService(Components.interfaces.nsIClipboard);
          var xferable = Components.classes["@mozilla.org/widget/transferable;1"].
                         createInstance(Components.interfaces.nsITransferable);
          xferable.addDataFlavor("text/unicode");
          var s = Components.classes["@mozilla.org/supports-string;1"].
                  createInstance(Components.interfaces.nsISupportsString);
          s.data = selection;
          xferable.setTransferData("text/unicode", s, selection.length * 2);
          clipboardSvc.setData(xferable, null, Components.interfaces.nsIClipboard.kGlobalClipboard);
        }
        if (aEvent.which == 120)
          sourceEditor.selectedText = "";
        break;*/
      default:
        break;
    }
  }
}

function BespinReplace()
{
  if (GetCurrentViewMode() == "source") {
    var sourceIframe = EditorUtils.getCurrentSourceEditorElement();
    var sourceEditor = sourceIframe.contentWindow.gEditor;
    if (sourceEditor.lastNeedle && sourceEditor.lastNeedle.from() && sourceEditor.lastNeedle.to()) {
      var end = sourceEditor.lastNeedle.to();
      sourceEditor.lastNeedle.replace(gDialog.bespinReplaceTextbox.value);
      sourceEditor.setCursor(end);
      //sourceEditor.focus();
    }
  }
  else
    ReplaceInWysiwyg();
}

function BespinReplaceAndFind()
{
  BespinReplace();
  BespinFind(true, false);
}

function BespinReplaceAll()
{
  var occurences = 0;
  if (GetCurrentViewMode() == "source") {
    var sourceIframe = EditorUtils.getCurrentSourceEditorElement();
    var sourceEditor = sourceIframe.contentWindow.gEditor;

    var query = gDialog.bespinFindTextbox.value;
    var isCaseSensitive = gDialog.bespinFindCaseSensitive.checked;
    sourceEditor.lastNeedle = sourceEditor.getSearchCursor(query, { line: 0, ch: 0 }, isCaseSensitive);

    while (sourceEditor.lastNeedle.findNext()) {
      occurences++;
      sourceEditor.lastNeedle.replace(gDialog.bespinReplaceTextbox.value);
    }
  }
  else {
    var found = true;
    var editor = EditorUtils.getCurrentEditor();
    editor.beginTransaction();
    while (found) {
      occurences++;
      ReplaceInWysiwyg();
      found = BespinFind(true, false);
    }
    editor.endTransaction();
  }
  var title = L10NUtils.getString("ReplaceAll");
  var msg = L10NUtils.getString("ReplacedPart1") +
            " " +
            occurences +
            " " +
            L10NUtils.getString("ReplacedPart2");
  Services.prompt.alert(null, title, msg);
}

function WysiwygShowFindBar()
{
  gDialog.bespinToolbox1.hidden = false;
  var editor = EditorUtils.getCurrentEditor();
  if (GetCurrentViewMode() == "wysiwyg") {
    var text = editor.outputToString("text/plain", 1).trim();
    if (text) {
      gDialog.bespinFindTextbox.value = text;
      BespinFind(true, true);
    }
  }
  else {
    var sourceIframe = EditorUtils.getCurrentSourceEditorElement();
    var sourceEditor = sourceIframe.contentWindow.gEditor;
    var text = sourceEditor.getSelection();
    if (text) {
      gDialog.bespinFindTextbox.value = text;
      BespinFind(true, true);
    }
  }
  gDialog.bespinFindTextbox.focus();
}

function CloseFindBar()
{
  gDialog.bespinToolbox1.hidden = true;
  gDialog.bespinToolbox2.hidden = true;
  window.content.focus();
  InContextHelper.showInContextPanel(InContextHelper.mElement);
}

function ReplaceInWysiwyg()
{
  var editor = EditorUtils.getCurrentEditor();

  // Does the current selection match the find string?
  var selection = editor.selection;

  var selStr = selection.toString();
  var specStr = gDialog.bespinFindTextbox.value;
  if (!gDialog.bespinFindCaseSensitive.checked)
  {
    selStr = selStr.toLowerCase();
    specStr = specStr.toLowerCase();
  }
  // Unfortunately, because of whitespace we can't just check
  // whether (selStr == specStr), but have to loop ourselves.
  // N chars of whitespace in specStr can match any M >= N in selStr.
  var matches = true;
  var specLen = specStr.length;
  var selLen = selStr.length;
  if (selLen < specLen)
    matches = false;
  else
  {
    var specArray = specStr.match(/\S+|\s+/g);
    var selArray = selStr.match(/\S+|\s+/g);
    if ( specArray.length != selArray.length)
      matches = false;
    else
    {
      for (var i=0; i<selArray.length; i++)
      {
        if (selArray[i] != specArray[i])
        {
          if ( /\S/.test(selArray[i][0]) || /\S/.test(specArray[i][0]) )
          {
            // not a space chunk -- match fails
            matches = false;
            break;
          }
          else if ( selArray[i].length < specArray[i].length )
          {
            // if it's a space chunk then we only care that sel be
            // at least as long as spec
            matches = false;
            break;
          }
        }
      }
    }
  }

  // If the current selection doesn't match the pattern,
  // then we want to find the next match, but not do the replace.
  // That's what most other apps seem to do.
  // So here, just return.
  if (!matches)
    return false;

  // nsPlaintextEditor::InsertText fails if the string is empty,
  // so make that a special case:
  var replStr = gDialog.bespinReplaceTextbox.value;
  if (replStr == "")
    editor.deleteSelection(0);
  else
    editor.insertText(replStr);

  return true;
}

