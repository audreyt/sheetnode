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

Drupal.sheetnode.resize = function() {
  if (this.sheet.SizeSSDiv()) {
    this.sheet.editor.ResizeTableEditor(this.sheet.width, this.sheet.height-
      (this.sheet.spreadsheetDiv.firstChild.offsetHeight + this.sheet.formulabarDiv.offsetHeight));
  }
}

$(document).ready(function() {
  $('#edit-submit').click(function() {
    $('#edit-save').val(Drupal.sheetnode.sheet.CreateSheetSave());
  });
  $(window).resize(function() {
    Drupal.sheetnode.resize();
  });
});

