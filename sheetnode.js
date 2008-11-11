// $Id$

Drupal.sheetnode = {};

Drupal.sheetnode.startUp = function() {
  SocialCalc.Constants.defaultImagePrefix = Drupal.settings.sheetnode.imageprefix;
  this.sheet = new SocialCalc.SpreadsheetControl();
  this.sheet.tabbackground="display:none;";
  this.sheet.toolbarbackground="display:none;";
  this.sheet.ParseSheetSave(Drupal.settings.sheetnode.value);
  this.sheet.InitializeSpreadsheetControl(Drupal.settings.sheetnode.element);
}

$(document).ready(function() {
  $('#edit-submit').click(function() {
    $('#edit-save').val(Drupal.sheetnode.sheet.CreateSheetSave());
  });
});

