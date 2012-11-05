/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is BlueGriffon.
 *
 * The Initial Developer of the Original Code is
 * Disruptive Innovations SARL.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <daniel.glazman@disruptive-innovations.com>, Original author
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://app/modules/editorHelper.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var InContextHelper = {

  mCancelNext: false,
  mElement: null,
  mStartScreenX: 0,
  mStartScreenY: 0,
  mResizing: false,

  mResizedElement: null,

  mIsEnabled: true,

  enable: function(aEnabled)
  {
    this.mIsEnabled = aEnabled;
  },

  isInContextEnabled: function()
  {
    var inContextEnabled = false;
    try {
      inContextEnabled = Services.prefs.getBoolPref("bluegriffon.floatingToolbar.enabled");
    }
    catch(e) {}
    return inContextEnabled && this.mIsEnabled;
  },

  hideInContextPanel: function()
  {
    if (!this.isInContextEnabled())
      return;

    gDialog.inContextStylePanel.hidePopup();
  },

  cancelNextInContextPanel: function() {
    this.mCancelNext = true;
  },

  showInContextPanel: function(aElement) {
    
    var delay = 1000;
    if (gDialog.inContextStylePanel.state != "closed") {
      this.onPopupHiding();
      delay = 0;
    }

    this.mElement = aElement;
    
    if (this.mElement && this.isInContextEnabled()) {
      var selectionRect = EditorUtils.getCurrentEditor().selection.getRangeAt(0).getBoundingClientRect();
      var elementRect = aElement.getBoundingClientRect();
      if (aElement.getAttribute("xmlns") == "http://disruptive-innovations.com/zoo/bluegriffon") {
        gDialog.elementStyling.hidden = true;
        gDialog.commentHacking.hidden = (aElement.nodeName != "comment");
        gDialog.phpHacking.hidden     = (aElement.nodeName != "php");
        gDialog.piHacking.hidden      = (aElement.nodeName != "pi");
      }
      else {
        gDialog.commentHacking.hidden = true;
        gDialog.phpHacking.hidden = true;
        gDialog.piHacking.hidden = true;
        gDialog.elementStyling.hidden = false;
        
      }
      setTimeout(this._showInContextPanel, delay, aElement, elementRect, selectionRect);
    }
    else {
      // for instance if we arrive here from the findbar
      this.enable(true);
    }
  },

  _showInContextPanel: function(aElement, elementRect, selectionRect) {
    if (InContextHelper.mCancelNext) {
      InContextHelper.mCancelNext = false;
      return;
    }
    gDialog.inContextStylePanel.openPopup(aElement, "after_pointer",
                                          selectionRect.left - elementRect.left,
                                          (elementRect.top < 0) ?  -elementRect.top : selectionRect.top - elementRect.top,
                                          false, false);
    if (!gDialog.commentHacking.hidden) {
      gDialog.commentHackingTextbox.value = aElement.lastChild.data;
      gDialog.commentHackingTextbox.focus();
    }
    else if (!gDialog.phpHacking.hidden) {
      gDialog.phpHackingTextbox.value = aElement.lastChild.data;
      gDialog.phpHackingTextbox.focus();
    }
    else if (!gDialog.piHacking.hidden) {
      gDialog.piDataHackingBox.value = aElement.lastChild.data;
      gDialog.piTargetHackingBox.value = aElement.lastChild.target;
      gDialog.piDataHackingBox.focus();
    }
  },

  onPopupHiding: function() {
    if (!this.mElement)
      return;
    if (!gDialog.commentHacking.hidden) {
      var comment = gDialog.commentHackingTextbox.value;
      var text = comment;
      if (text.length > 22)
        text = text.substr(0, 22) + "...";
      if (this.mElement.lastChild.data != comment) {
        // update
        var editor = EditorUtils.getCurrentEditor();
        editor.beginTransaction();
        editor.setAttribute(this.mElement, "title", text);
	      var txn = new diCommentOrPIChangeTxn(this.mElement.lastChild, comment);
	      editor.transactionManager.doTransaction(txn);
        editor.endTransaction();
      }
    }
    else if (!gDialog.phpHacking.hidden) {
      var data = gDialog.phpHackingTextbox.value;
      var text = data;
      if (text.length > 22)
        text = text.substr(0, 22) + "...";
      if (this.mElement.lastChild.data != data) {
        // update
        var editor = EditorUtils.getCurrentEditor();
        editor.beginTransaction();
        editor.setAttribute(this.mElement, "title", text);
        var txn = new diCommentOrPIChangeTxn(this.mElement.lastChild, data);
        editor.transactionManager.doTransaction(txn);
        editor.endTransaction();
      }
    }
    else if (!gDialog.piHacking.hidden) {
      var data   = gDialog.piDataHackingBox.value;
      var target = gDialog.piTargetHackingBox.value;
      var text = target + " " + data;
      if (text.length > 22)
        text = text.substr(0, 22) + "...";
      if (this.mElement.lastChild.data != data) {
        // update
        var editor = EditorUtils.getCurrentEditor();
        editor.beginTransaction();
        editor.setAttribute(this.mElement, "title", text);
        var txn = new diCommentOrPIChangeTxn(this.mElement.lastChild, data);
        editor.transactionManager.doTransaction(txn);
        editor.endTransaction();
      }
    }
  },

  startResizing: function(aEvent, aElt) {
    this.mResizedElement = gDialog[aElt.getAttribute("element")];
    this.mStartScreenX = aEvent.screenX;
    this.mStartScreenY = aEvent.screenY;
    this.mStartWidth = this.mResizedElement.boxObject.width;
    this.mStartHeight = this.mResizedElement.boxObject.height;
    this.mResizing = true;
    aElt.setCapture(false);
  },

  moveResizer: function(aEvent, aElt) {
    if (!this.mResizing)
      return;
    var screenX = aEvent.screenX;
    var screenY = aEvent.screenY;
    this.mResizedElement.style.width = (this.mStartWidth + screenX - this.mStartScreenX) + "px";
    this.mResizedElement.style.height = (this.mStartHeight + screenY - this.mStartScreenY) + "px";
  },

  stopResizing: function(aEvent, aElt) {
    this.mResizing = false;
    aElt.releaseCapture();
    document.persist(this.mResizedElement.id, "style");
  }
}
