// $Id$

Drupal.sheetnode = {};

Drupal.sheetnode.startUp = function(str, ele) { 
  this.sheet = new SocialCalc.Sheet();
  this.sheet.ParseSheetSave(str);
  this.context = new SocialCalc.RenderContext(this.sheet);
  this.context.showGrid = true;
  this.context.showRCHeaders = true;
  this.context.CalculateCellSkipData();
  this.editor = new SocialCalc.TableEditor(this.context);
  this.editor.imageprefix = Drupal.settings.sheetnode.imageprefix;
  this.sizes = SocialCalc.GetViewportInfo();
  this.teelement = this.editor.CreateTableEditor(this.sizes.width-20, this.sizes.height-165);
  ele.replaceChild(this.teelement, ele.firstChild);
  this.editor.SchedulePositionCalculations();
}

Drupal.sheetnode.save = function(ele) {
  ele.value = this.sheet.CreateSheetSave();
}

$(document).ready(function() {
  $('#edit-submit').click(function() {
    Drupal.sheetnode.save($('#edit-save')[0]);
  });
});

