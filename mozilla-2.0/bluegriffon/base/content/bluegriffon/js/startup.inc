Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://app/modules/urlHelper.jsm");
Components.utils.import("resource://app/modules/editorHelper.jsm");
Components.utils.import("resource://app/modules/bgQuit.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/fileChanges.jsm");

var gContentWindow = null;

function Startup()
{
  SetLocationDB();
  var url = null;
  // do we have a URL to open ?
  try {
    if (window.arguments && window.arguments[0]) {
      var cl = window.arguments[0];
      if (cl && cl instanceof Components.interfaces.nsISupportsArray) {
        url = cl.GetElementAt(0).QueryInterface(Components.interfaces.nsISupportsString).data;
        var localFile = UrlUtils.newLocalFile(url);
        var ioService =
          Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);
        var fileHandler =
          ioService.getProtocolHandler("file")
                   .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
        url = fileHandler.getURLSpecFromFile(localFile);
      }
    }
    else if (window.arguments.length >= 2 &&
             window.arguments[1])
      url = window.arguments[1];
  }
  catch(e) { url = null; }

  GetUIElements();

  ComposerCommands.setupMainCommands();
  window.updateCommands("style");
  RestoreShortcuts();

  InstallCustomizationDoneCallbacks();

  gDialog.structurebar.init();

  ResizeEventNotifier.startup();
  EditorScrolledNotifier.startup();
  BGZoomManager.startup();
  BGUpdateManager.check();
  
  initLocalFontFaceMenu(gDialog.FontFacePopup);
  initLocalFontFaceMenu(gDialog.inContextFontFacePopup);

  try {
    var pbi = Services.prefs.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
    pbi.addObserver("bluegriffon.", BlueGriffonPrefsObserver, false);
  } catch(ex) {
  }

  // let's finish with the url
  if (url)
    setTimeout(OpenFile, 100, url, true);

  setTimeout(LoadLastTabs, 1000);
  BlueGriffonQuitHelper.init();

  if (document.getElementById("tipoftheday"))
    document.getElementById("tipoftheday").launch(null);

  AddonManager.getAddonByID("{f13b157f-b174-47e7-a34d-4815ddfdfeb8}",
    function(addon) { if(addon) document.getElementById("venkmanMenuitem").hidden = false; } );

  AddonManager.getAddonByID("inspector@mozilla.org",
    function(addon) { if(addon) document.getElementById("menu_inspector").hidden = false; } );

  NotifierUtils.addNotifierCallback("selection_strict", BlueGriffon_lookForFileChanges, this);

  window.addEventListener("sizemodechange", onSizeModeChange, false);
}

var gLookingForFileChangesTimeout = null;

function BlueGriffon_lookForFileChanges() {
  if (gLookingForFileChangesTimeout)
    clearTimeout(gLookingForFileChangesTimeout);

  gLookingForFileChangesTimeout = setTimeout(function() { FileChangeUtils.lookForChanges(); }, 500);
}

var gLastTabsArray;
function LoadLastTabs()
{
  var lastTabs = "";
  gLastTabsArray = [];
  try {
    lastTabs = Services.prefs.getCharPref("bluegriffon.defaults.lastTabs");
    Services.prefs.setCharPref("bluegriffon.defaults.lastTabs", "");
  }
  catch(e) {}
  if (lastTabs) {
    gLastTabsArray = lastTabs.split("|");
    //gLastTabsArray.reverse();
    NotifierUtils.addNotifierCallback("tabCreated", OpenTabFromPreviousSession);
    var url = gLastTabsArray.pop();
    OpenFile(url, true);
  }
}

function OpenTabFromPreviousSession(aReason, aContext)
{
  if (gLastTabsArray.length) {
    var url = gLastTabsArray.pop();
    OpenFile(url, true);
  }
  else {
    NotifierUtils.removeNotifierCallback("tabCreated", OpenTabFromPreviousSession);
  }
}

function RestoreShortcuts()
{
  try {
    var file = Components.classes["@mozilla.org/file/directory_service;1"]
                         .getService(Components.interfaces.nsIProperties)
                         .get("ProfD", Components.interfaces.nsIFile);
    file.append("shortcuts.sqlite");
    
    var storageService = Components.classes["@mozilla.org/storage/service;1"]
                            .getService(Components.interfaces.mozIStorageService);
    var dbConn = storageService.openDatabase(file);

    var statement = dbConn.createStatement("SELECT * from shortcuts");

    // remove all existing shortcuts...
    var elts = document.querySelectorAll("#composer-main-menubar *[key], toolbar *[key]");
    for (var i = 0; i < elts.length; i++) {
      var elt = elts[i];
      var keyId = elt.getAttribute("key");
      elt.removeAttribute("key");
      var keyElt = document.getElementById(keyId);
      if (keyElt) { //sanity check
        var keyset = keyElt.parentNode;
        keyset.removeChild(keyElt);
        var parent = keyset.parentNode;
        var nextSibling = parent.nextSibling;
        parent.removeChild(keyset);
        parent.insertBefore(keyset, nextSibling);
       }
    }

    while (statement.executeStep()) {
      var selector = statement.getUTF8String(1);
      var modifiers = statement.getUTF8String(2);
      var key = statement.getUTF8String(3);

      var elt = document.querySelector(selector);
      // recreate a key
      var keyset = document.getElementById("mainKeySet");
      keyElt = document.createElement("key");
      keyElt.setAttribute("modifiers", modifiers);
      if (key.length == 1)
        keyElt.setAttribute("key", key);
      else
        keyElt.setAttribute("keycode", key);
      if (elt.hasAttribute("command"))
        keyElt.setAttribute("command", elt.getAttribute("command"));
      if (elt.hasAttribute("oncommand"))
        keyElt.setAttribute("oncommand", elt.getAttribute("oncommand"));
      if (!elt.hasAttribute("command") && !elt.hasAttribute("oncommand")) {
        var cmdStr = 'var e = document.createEvent("Events"); e.initEvent("command", true, true); document.querySelector("'
                     + selector
                     +'").dispatchEvent(e);';
        keyElt.setAttribute("oncommand", cmdStr);
      }

      var keyId =  "key-" + modifiers.replace( /,/g , "-") + key;
      keyElt.setAttribute("id", keyId);
      keyset.appendChild(keyElt);
      elt.setAttribute("key", keyId);

      var parent = keyset.parentNode;
      var nextSibling = parent.nextSibling;
      parent.removeChild(keyset);
      parent.insertBefore(keyset, nextSibling);
    }
    dbConn.close();
  }
  catch(e) {
    // no shortcut DB yet, do nothing
  }
}
