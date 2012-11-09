/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  showTabView(function () {
    let tab = gBrowser.addTab();
    registerCleanupFunction(function () gBrowser.removeTab(tab));

    whenAppTabIconAdded(function() {
      let cw = TabView.getContentWindow();
      let body = cw.document.body;
      let [appTabIcon] = cw.iQ(".appTabTray .appTabIcon");

      EventUtils.synthesizeMouseAtCenter(appTabIcon, {type: "mousedown"}, cw);
      EventUtils.synthesizeMouse(body, 500, 100, {type: "mousemove"}, cw);
      EventUtils.synthesizeMouse(body, 500, 100, {type: "mouseup"}, cw);

      ok(TabView.isVisible(), "tabview is still visible");

      hideTabView(finish);
    });
    gBrowser.pinTab(tab);
  });
}