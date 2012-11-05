var DOWNLOAD_URL = "http://bluegriffon.org/pages/Download";

function ShowUpdatePage()
{
  loadExternalURL(DOWNLOAD_URL);
  window.close();
}

function Startup()
{
  GetUIElements();

  var message = window.arguments[0];
  var messageURL = window.arguments[1];
  if (message) {
    document.title = "BlueGriffon";
    gDialog.warningMessage.firstChild.textContent = message;
    if (messageURL)
      DOWNLOAD_URL = messageURL;
    else
      gDialog.downloadButton
  }
}