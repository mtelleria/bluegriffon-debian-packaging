RegisterIniter(FlexBoxSectionIniter);

function FlexBoxSectionIniter(aElt, aRuleset)
{
  var d = CssInspector.getCascadedValue(aRuleset, "display");
  gDialog.flexBoxEnabledCheckbox.checked = (d == "-moz-box" || d == "-moz-inline-box");
  gDialog.inlineBoxCheckbox.disabled = !gDialog.flexBoxEnabledCheckbox.checked;
  gDialog.inlineBoxCheckbox.checked = (d == "-moz-inline-box");

  var ba = CssInspector.getCascadedValue(aRuleset, "-moz-box-align");
  CheckToggle(gDialog.startBoxAlignButton,    ba == "start");
  CheckToggle(gDialog.centerBoxAlignButton,   ba == "center");
  CheckToggle(gDialog.endBoxAlignButton,      ba == "end");
  CheckToggle(gDialog.baselineBoxAlignButton, ba == "baseline");
  CheckToggle(gDialog.stretchBoxAlignButton,  ba == "stretch");

  var bd = CssInspector.getCascadedValue(aRuleset, "-moz-box-direction");
  CheckToggle(gDialog.normalBoxDirectionButton,    bd == "normal");
  CheckToggle(gDialog.reverseBoxDirectionButton,   bd == "reverse");

  var bf = CssInspector.getCascadedValue(aRuleset, "-moz-box-flex");
  gDialog.boxFlexTextbox.value = bf;

  var bo = CssInspector.getCascadedValue(aRuleset, "-moz-box-orient");
  CheckToggle(gDialog.horizontalBoxOrientButton,   bo == "horizontal");
  CheckToggle(gDialog.verticalBoxOrientButton,     bo == "vertical");

  var bog = CssInspector.getCascadedValue(aRuleset, "-moz-box-ordinal-group");
  gDialog.boxOrdinalGroupTextbox.value = bog;

  var bp = CssInspector.getCascadedValue(aRuleset, "-moz-box-pack");
  CheckToggle(gDialog.startBoxPackButton,    bp == "start");
  CheckToggle(gDialog.centerBoxPackButton,   bp == "center");
  CheckToggle(gDialog.endBoxPackButton,      bp == "end");
  CheckToggle(gDialog.justifyBoxPackButton,  bp == "justify");
}

function ToggleFlexBox()
{
  gDialog.inlineBoxCheckbox.disabled = !gDialog.flexBoxEnabledCheckbox.checked;
  ApplyStyles( [
                 {
                   property: "display",
                   value: gDialog.flexBoxEnabledCheckbox.checked ?
                          (gDialog.inlineBoxCheckbox.checked ? "-moz-inline-box" : "-moz-box") :
                          ""
                 }
               ]);
}
