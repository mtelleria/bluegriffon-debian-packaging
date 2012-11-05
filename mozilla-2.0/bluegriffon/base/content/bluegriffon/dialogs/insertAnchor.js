Components.utils.import("resource://app/modules/cssHelper.jsm");
Components.utils.import("resource://app/modules/editorHelper.jsm");
Components.utils.import("resource://app/modules/urlHelper.jsm");

var gDoc = null;
var gNode = null;
var gIsHTML5 = false;

function Startup()
{
  gNode = window.arguments[0];

  GetUIElements();

  InitDialog();
}

function InitDialog()
{
  document.documentElement.getButton("accept").setAttribute("disabled", "true");

  gDoc = EditorUtils.getCurrentDocument()
  gIsHTML5 = (gDoc.doctype && gDoc.doctype.publicId == "");

  var ids = CssUtils.getAllIdsForDocument(gDoc);
  for (var i = 0; i < ids.length; i++) {
    var id = ids[i];
    if (!gDoc.getElementById(id)
        && (gIsHTML5 || !gDoc.querySelector("a[name='" + id + "']"))) {
      gDialog.anchorNameMenulist.appendItem(id, id);
    }
  }

  if (gNode) {
    if (gNode.id)
      gDialog.anchorNameMenulist.value = gNode.id;
    else if (!gIsHTML5 && gNode.nodeName.toLowerCase() == "a" && gNode.hasAttribute("name"))
      gDialog.anchorNameMenulist.value = gNode.getAttribute("name");
    gOriginalAnchor = gDialog.anchorNameMenulist.value;
  }
}

function UpdateButtons()
{
  var id = gDialog.anchorNameMenulist.value;
  if (gNode && id == gOriginalAnchor) {
    document.documentElement.getButton("accept").removeAttribute("disabled");
    return;
  }
  
  if (!id
      || gDoc.getElementById(id)
      || (!gIsHTML5 && gDoc.querySelector("a[name='" + id + "']")))
    document.documentElement.getButton("accept").setAttribute("disabled", "true");
  else
    document.documentElement.getButton("accept").removeAttribute("disabled");
}

function onAccept()
{
  var id = gDialog.anchorNameMenulist.value;
  var editor = EditorUtils.getCurrentEditor();
  if (gNode) {
    if (gOriginalAnchor) {
      if (gNode.id == gOriginalAnchor)
        gNode.id = id;
      else if (gNode.nodeName.toLowerCase() == "a" && gNode.getAttribute("name") == gOriginalAnchor)
        gNode.setAttribute("name", id);
    }
    else
      gNode.id = id;
  }
  else {
    var isCollapsed = editor.selection.isCollapsed;
    if (isCollapsed) {
      editor.beginTransaction();
      var anchor = editor.document.createElement("a")
      anchor.setAttribute("name", id);
      try {
        editor.insertElementAtSelection(anchor, false);
        editor.endTransaction();
        if (gIsHTML5) {
          editor.setAttribute(anchor, "id", id);
          //editor.removeAttribute(anchor, "name");
          //editor.setCaretAfterElement(anchor);
        }
      }
      catch (e) {}
    }
    else
      editor.setInlineProperty("a", gIsHTML5 ? "id" : "name", id)
  }
}

