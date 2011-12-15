<?php

/**
 *
 * @param type $format
 * @param type $form_state
 * @return string 
 */
function _sheetnode_phpexcel_import_form($format, &$form_state) {
  $form['#attributes'] = array('enctype' => "multipart/form-data");
  
  // @todo - This is a hack - is this the best way to get the format to 
  // be imported?
  $format = $form_state['build_info']['args'][0];

  $plugins = sheetnode_phpexcel_get_plugins();
  $form['#plugin'] = $plugins[$format];
  $form['import'] = array(
    '#type' => 'file',
    '#title' => filter_xss(t('!format to import', array('!format' => $form['#plugin']['long-name']))),
    '#size' => 40,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );
  return $form;
}

/**
 *
 * @param type $form
 * @param type $form_state
 * @return type 
 */
function _sheetnode_phpexcel_import_form_validate($form, &$form_state) {
  $source = $_FILES['files']['name']['import'];

  // Handle PHP file upload errors.
  switch ($_FILES['files']['error']['import']) {
  case UPLOAD_ERR_OK:
    break;
  case UPLOAD_ERR_FORM_SIZE:
  case UPLOAD_ERR_INI_SIZE:
    form_set_error('import', t('The file %file could not be saved, because it exceeds %maxsize, the maximum allowed size for uploads. Please choose a smaller file or notify your site administrator.', 
      array('%file' => $source, '%maxsize' => format_size(file_upload_max_size()))));
    return;
  case UPLOAD_ERR_PARTIAL:
    form_set_error('import', t('The file %file could not be saved, because the upload did not complete. Please try again or notify your site administrator.', array('%file' => $source)));
    return;
  case UPLOAD_ERR_NO_FILE:
    form_set_error('import', t('No file was uploaded. Please choose a file to upload.'));
    return;
  case UPLOAD_ERR_NO_TMP_DIR:
    form_set_error('import', t('The file %file could not be saved, because the temporary upload folder is missing. Please notify your site administrator.', array('%file' => $source)));
    return;
  case UPLOAD_ERR_CANT_WRITE:
    form_set_error('import', t('The file %file could not be saved, because a failure occurred while writing it to the temporary folder. Please notify your site administrator.', array('%file' => $source)));
    return;
  case UPLOAD_ERR_EXTENSION:
    form_set_error('import', t('The file %file could not be saved, because its extension is disallowed. Please notify your site administrator.', array('%file' => $source)));
    return;
  default:
    form_set_error('import', t('The file %file could not be saved. An unknown error %error has occurred. Please notify your site administrator.', 
      array('%file' => $source, '%error' => $_FILES['files']['error']['import'])));
    return;
  }

  // TODO: Validate that file is of type $form['#plugin'].
}

/**
 *
 * @global type $user
 * @param type $form
 * @param array $form_state  
 */
function _sheetnode_phpexcel_import_form_submit($form, &$form_state) {
  require_once(variable_get('sheetnode_phpexcel_library_path', '') . '/Classes/PHPExcel.php');
  $validators = array();
  $validators['file_validate_extensions'] = "";
  $file = file_save_upload('import', $validators);  // Set array to allow the XLSX extension or an empty array to allow all
  $batch = array();

  sheetnode_phpexcel_batch_import(array($file->uri), NULL, '_sheetnode_phpexcel_import_callback', $form_state);
}

/**
 *
 * @global type $user
 * @param type $node
 * @param array $params  
 * @param array $context
 */
function _sheetnode_phpexcel_import_callback($node, $params, &$context) {
  if ($context['sandbox']['total'] > 1 && module_exists('book')) {
    // Create book if none present.
    if (empty($context['sandbox']['book'])) {
      global $user;
      $book = new StdClass;
      $book->type = 'book';

      node_object_prepare($book);

      $book->title = $workbook->getProperties()->getTitle();
      if (empty($book->title)) {
        $book->title = t('Untitled Workbook');
      }
      $book->name = $user->name;
      $book->book['bid'] = 'new';
      $book->book['plid'] = $book->book['mlid'] = NULL;

      // Let other modules alter the book or do other work.
      drupal_alter('sheetnode_import', $book, $params, $context);

      $book = node_submit($book);
      node_save($book);
      if ($book->nid) {
        $context['results'][] = $book->nid;
        $context['sandbox']['book'] = $book;
      }
    }

    // Mark the book as parent to the sheetnode.
    $book = $context['sandbox']['book'];
    $node->book['bid'] = $book->nid;
    $node->book['plid'] = $book->book['mlid'];
    $node->book['module'] = 'book';
    $node->book['weight'] = $context['sandbox']['current'] - 1;
  }
}

/**
 * 
 * @global type $user
 * @param type $filename
 * @param type $callback
 * @param type $params - see below for how this is used.
 * @param type $context 
 */
function _sheetnode_phpexcel_batch_import_file($filename, $callback, $params, &$context) {
  require_once(variable_get('sheetnode_phpexcel_library_path', '') . '/Classes/PHPExcel.php');
  module_load_include('inc', 'node', 'node.pages');
  set_time_limit(0);
  global $user;

  //  We have "temporary://" in the filename, but Drupal doesn't replace
  //  that in subsequent processing. Therefore, we replace the "temporary://"
  //  in the filename here so that the file will get processed correctly. 
  $temp_dir = file_directory_temp() . '/';
  $filename = str_replace("temporary://", $temp_dir, $filename);
  
  //  Use $params as a filter (if set) to load only sheet(s) whose titles 
  //  match the given names (case-insensitive matching). If this is the case, 
  //  $params has been set from the $form_state array. We expect the following
  //  protocol for the parameters. 
  //  $params['build_info']['args'][1] holds number of sheets to import (=n) 
  //  and then the next n values hold the names of the sheet labels to be 
  //  matched. Finally, later processing checks to see if there is another 
  //  parameter following the last filter sheet name. If there is another 
  //  parameter (e.g., 'dest'), then it is used for the final destination URL, 
  //  such that the final destination URL will be http://example.com/dest/nid. 
  $num_filter_sheets = 0;
  $filter_sheet_names = array();
  
  if (isset($params['build_info']['args'][1]))  {
    if (is_numeric($params['build_info']['args'][1]))  {
      $num_filter_sheets = $params['build_info']['args'][1];
    }
  }

  //  Fill the $filter_sheet_names array with the sheet names that will be 
  //  used for filtering
  $i = 0; 
  $n = 2;
  while ($n < $num_filter_sheets + 2) {
    if (isset($params['build_info']['args'][$n]))  {
      $filter_sheet_names[$i] = $params['build_info']['args'][$n];
      $i++;
    }
    $n++;
  }

  $last_node_id = 0;
    
  if (empty($context['sandbox']['workbook'])) {
    // Load workbook and get number of worksheets.
    $context['sandbox']['workbook'] = $workbook = PHPExcel_IOFactory::load($filename);
    $context['sandbox']['total'] = $workbook->getSheetCount();
    $context['sandbox']['current'] = 0;
  } 
  else {
    // Create sheetnode out of current sheet.
    $workbook = PHPExcel_IOFactory::load($filename);
    $context['sandbox']['workbook'] = $workbook;
    $sheet = $workbook->getSheet($context['sandbox']['current'] - 1);
    
    // Import sheet if $num_filter_sheets = 0 or title of current sheet
    // matches a value in the array of $filter_sheet_names.
    if (($num_filter_sheets == 0) || (_sheetnode_phpexcel_title_matches($sheet->getTitle(), $filter_sheet_names)))  {

      $node = new StdClass;
      $node->type = 'sheetnode';

      node_object_prepare($node);

      //  Set the imported node name as (short) filename >> sheetname 
      $pos = strrpos($filename, '/');
      $prefix = substr($filename, $pos + 1);
      $node->title = $prefix . ' >> ' . $sheet->getTitle();
      $node->name = $user->name;
      $node->sheetnode['value'] = _sheetnode_phpexcel_import_do($workbook, $sheet);
      $node->sheetnode['template'] = NULL;

      // Let other modules alter the sheetnode or do other work.
      if (!empty($callback) && function_exists($callback)) {
        $callback($node, $params, $context);
      }
      drupal_alter('sheetnode_import', $node, $params, $context);

      // Save the sheetnode.
      $node = node_submit($node);
      node_save($node);
      if (!empty($node->nid)) {
        $context['results'][] = $node->nid;
        _sheetnode_save($node->nid, $node->vid, $node->sheetnode['value']);
        
        //  If the calling function set the redirect, then set it in the batch 
        //  array. For example, if final parameter is 'dest', then it is used 
        //  for the final destination URL, such that the final destination URL 
        //  will be http://example.com/dest/nid, where nid is the current node ID.
        if (isset($params['build_info']['args'][$n]))  {
          $batch = & batch_get();
          $batch['redirect'] = $params['build_info']['args'][$n] . '/' . $node->nid;
        }
      }
    }
  }

  // Update progress information.
  if ($context['sandbox']['current'] < $context['sandbox']['total']) {
    $sheet = $workbook->getSheet($context['sandbox']['current']);
    $context['message'] = t('Now processing sheet %sheet.', array('%sheet' => $sheet->getTitle()));
    $context['finished'] = $context['sandbox']['current'] / $context['sandbox']['total'];
    $context['sandbox']['current']++;
  }
}

/**
 *
 * @param type $success
 * @param type $results
 * @param type $operations
 */
function _sheetnode_phpexcel_batch_import_finished($success, $results, $operations) {
  $batch = & batch_get();
  if (empty($batch['redirect']) && !empty($results)) {
    drupal_goto('node/' . $results[0]);
  }
}

/**
 *
 * @param type $workbook
 * @param type $sheet
 * @return type 
 */
function _sheetnode_phpexcel_import_do($workbook, $sheet) {
  require_once(drupal_get_path('module', 'sheetnode') . '/socialcalc.inc');
  $sc = array(); // SocialCalc array structure

  // Default values.
  $font = $sheet->getDefaultStyle()->getFont();
  $defaultcolwidth = PHPExcel_Shared_Drawing::cellDimensionToPixels($sheet->getDefaultColumnDimension()->getWidth(), $font);
  if ($defaultcolwidth > 0) {
    $sc['attribs']['defaultcolwidth'] = $defaultcolwidth;
  }
  else {
    $sc['attribs']['defaultcolwidth'] = 30;
  }
  $defaultrowheight = $sheet->getDefaultRowDimension()->getRowHeight();
  if ($defaultrowheight > 0) {
    $sc['attribs']['defaultrowheight'] = $defaultrowheight;
  }
  else {
    $sc['attribs']['defaultrowheight'] = 20;
  }
  $sc['attribs']['lastrow'] = $sheet->getHighestRow();
  $sc['attribs']['lastcol'] = PHPExcel_Cell::columnIndexFromString($sheet->getHighestColumn());

  // Iterate on rows.
  foreach ($rit = $sheet->getRowIterator() as $row) {
    $r = $row->getRowIndex();
    $height = $sheet->getRowDimension($r)->getRowHeight();
    if ($height != -1 && $height != $sc['attribs']['defaultrowheight']) {
      $sc['rowattribs']['height'][$r] = $height;
    }
    if (!$sheet->getRowDimension($r)->getVisible()) {
      $sc['rowattribs']['hide'][$r] = TRUE;
    }

    // Iterate on cells.
    foreach ($cit = $row->getCellIterator() as $cell) {
      $ca = $cell->getColumn();
      $c = PHPExcel_Cell::columnIndexFromString($ca);
      $width = $sheet->getColumnDimension($ca)->getWidth();
      if ($width != -1) {
        $width = PHPExcel_Shared_Drawing::cellDimensionToPixels($width, $font);
        if ($width != $sc['attribs']['defaultcolwidth']) {
          $sc['colattribs']['width'][$c] = $width;
        }
      }
      if (!$sheet->getColumnDimension($ca)->getVisible()) {
        $sc['colattribs']['hide'][$c] = TRUE;
      }
      
      _sheetnode_phpexcel_import_cell($workbook, $sheet, $cell, $sc);
    }
  }

  // Cell merges
  foreach ($sheet->getMergeCells() as $range) {
    list($first, $last) = explode(':', $range);
    if (!isset($sc['cells'][$first])) continue;
    $firstcr = socialcalc_coord_to_cr($first);
    $lastcr = socialcalc_coord_to_cr($last);
    $sc['cells'][$first]['colspan'] = $lastcr[0] - $firstcr[0] + 1;
    $sc['cells'][$first]['rowspan'] = $lastcr[1] - $firstcr[1] + 1;
  }

  // Names
  foreach ($workbook->getNamedRanges() as $range) {
    // TODO: Handle external references and functions.
    $sc['names'][$range->getName()] = array(
      'desc' => '',
      'definition' => $range->getRange(),
    );
  }
  
  // Reset caches.
  _sheetnode_phpexcel_get_cached_styles(TRUE);

  $socialcalc = array(
    'sheet' => $sc,
    'edit' => socialcalc_default_edit($sc),
    'audit' => socialcalc_default_audit($sc),
  );
  return socialcalc_save($socialcalc);
}

/**
 *
 * @param type $workbook
 * @param type $sheet
 * @param type $cell
 * @param array $sc 
 */
function _sheetnode_phpexcel_import_cell($workbook, $sheet, $cell, &$sc) {
  $c = array(); // output cell
  
  // Position
  $c['pos'] = array(PHPExcel_Cell::columnIndexFromString($cell->getColumn()), $cell->getRow());

  // Value and type
  switch ($cell->getDataType()) {
  case PHPExcel_Cell_DataType::TYPE_BOOL:
    $c['datavalue'] = (bool)$cell->getValue();
    $c['datatype'] = 'v';
    $c['valuetype'] = 'n';
    break;
  case PHPExcel_Cell_DataType::TYPE_NUMERIC:
    $c['datavalue'] = floatval($cell->getValue());
    $c['datatype'] = 'v';
    $c['valuetype'] = 'n';
    break;
  case PHPExcel_Cell_DataType::TYPE_STRING:
    $text = $cell->getValue();
    if ($text instanceof PHPExcel_RichText) {
      $style = $sheet->getStyle($cell->getCoordinate());
      $defaultfont = $style->getFont();
      foreach ($text->getRichTextElements() as $element) {
        $font = $element instanceof PHPExcel_RichText_Run ? $element->getFont() : $defaultfont;
        $cellData .= '<span style="' . _sheetnode_phpexcel_assembleCSS(_sheetnode_phpexcel_createCSSStyleFont($font)) . '">';

        if ($font->getSuperScript()) {
          $cellData .= '<sup>';
        } else if ($font->getSubScript()) {
          $cellData .= '<sub>';
        }

        // Convert UTF8 data to PCDATA
        $cellText = $element->getText();
        $cellData .= htmlspecialchars($cellText);

        if ($font->getSuperScript()) {
          $cellData .= '</sup>';
        } else if ($font->getSubScript()) {
          $cellData .= '</sub>';
        }

        $cellData .= '</span>';
      }
      $c['datavalue'] = $cellData;
      $c['valuetype'] = 'th';
    }
    else { // it is plain text
      $c['datavalue'] = $text;
      $c['valuetype'] = 't';
    }
    $c['datatype'] = 't';
    break;
  case PHPExcel_Cell_DataType::TYPE_FORMULA:
    $c['datatype'] = 'f';
    $c['formula'] = ltrim($cell->getValue(), '=');
    try {
      $c['datavalue'] = $cell->getCalculatedValue();
    }
    catch (Exception $e) {
      $c['datavalue'] = 0;
      watchdog('sheetnode_phpexcel', 'Error importing formula "!formula" at cell !coord.', array('!formula' => $c['formula'], '!coord' => $cell->getCoordinate()), WATCHDOG_WARNING);
    }
    $c['valuetype'] = is_numeric($c['datavalue']) ? 'n' : 't';
    break;
  case PHPExcel_Cell_DataType::TYPE_ERROR:
    $c['errors'] = $cell->getValue();
    break;
  case PHPExcel_Cell_DataType::TYPE_NULL:
    break;
  }

  // Comment
  $comment = $sheet->getComment($cell->getCoordinate());
  if (!empty($comment)) {
    $c['comment'] = $comment->getText()->getPlainText();
  }

  // Style
  $style = $sheet->getStyle($cell->getCoordinate());
  $styles = _sheetnode_phpexcel_get_cached_styles();
  $hash = $style->getHashCode();
  if (empty($styles[$hash])) {
    // Font
    $font = _sheetnode_phpexcel_get_font($style->getFont());
    if ($font) {
      $styles[$hash]['font'] = _sheetnode_phpexcel_insert_hash($sc, 'font', $font);
    }

    // Color
    $color = _sheetnode_phpexcel_get_color($style->getFont()->getColor());
    if ($color) {
      $styles[$hash]['color'] = _sheetnode_phpexcel_insert_hash($sc, 'color', $color);
    }

    // Background color
    $bgcolor = _sheetnode_phpexcel_get_color($style->getFill()->getStartColor());
    if ($bgcolor) {
      $styles[$hash]['bgcolor'] = _sheetnode_phpexcel_insert_hash($sc, 'color', $bgcolor);
    }

    // Borders
    $borders = array(
      'bt' => 'getTop',
      'bl' => 'getLeft',
      'bb' => 'getBottom',
      'br' => 'getRight',
    );
    foreach ($borders as $bi => $method) {
      $border = _sheetnode_phpexcel_get_border($style->getBorders()->$method());
      if ($border) {
        $styles[$hash][$bi] = _sheetnode_phpexcel_insert_hash($sc, 'borderstyle', $border);
      }
    }

    // Cell format (horizontal alignment)
    $cellformat = _sheetnode_phpexcel_get_cellformat($style);
    if ($cellformat) {
      $styles[$hash]['cellformat'] = _sheetnode_phpexcel_insert_hash($sc, 'cellformat', $cellformat);
    }

    // Layout (padding + vertical alignment)
    $layout = _sheetnode_phpexcel_get_layout($style);
    if ($layout) {
      $styles[$hash]['layout'] = _sheetnode_phpexcel_insert_hash($sc, 'layout', $layout);
    }

    // Number format
    $ntvf = _sheetnode_phpexcel_get_valueformat($style->getNumberFormat());
    if ($ntvf) {
      $styles[$hash]['nontextvalueformat'] = _sheetnode_phpexcel_insert_hash($sc, 'valueformat', $ntvf);
    }
  }
  $c += $styles[$hash]; // merge the cached styles into the cell directly.

  $sc['cells'][$cell->getCoordinate()] = $c;
}

/**
 *
 * @staticvar array $styles
 * @param type $reset
 * @return array 
 */
function _sheetnode_phpexcel_get_cached_styles($reset = FALSE) {
  static $styles = array();
  if ($reset) {
    $styles = array();
  }
  return $styles;
}

/**
 * LIFTED FROM PHPExcel/Classes/PHPExcel/Writer/HTML.php
 * Takes array where of CSS properties / values and converts to CSS string
 *
 * @param array
 * @return string
 */
function _sheetnode_phpexcel_assembleCSS($pValue = array())
{
  $pairs = array();
  foreach ($pValue as $property => $value) {
    $pairs[] = $property . ':' . $value;
  }
  $string = implode('; ', $pairs);

  return $string;
}

/**
 * LIFTED FROM PHPExcel/Classes/PHPExcel/Writer/HTML.php
 * Create CSS style (PHPExcel_Style_Font)
 *
 * @param PHPExcel_Style_Font     $pStyle     PHPExcel_Style_Font
 * @return  array
 */
function _sheetnode_phpexcel_createCSSStyleFont(PHPExcel_Style_Font $pStyle) {
  // Construct CSS
  $css = array();

  // Create CSS
  if ($pStyle->getBold()) {
    $css['font-weight'] = 'bold';
  }
  if ($pStyle->getUnderline() != PHPExcel_Style_Font::UNDERLINE_NONE && $pStyle->getStrikethrough()) {
    $css['text-decoration'] = 'underline line-through';
  } else if ($pStyle->getUnderline() != PHPExcel_Style_Font::UNDERLINE_NONE) {
    $css['text-decoration'] = 'underline';
  } else if ($pStyle->getStrikethrough()) {
    $css['text-decoration'] = 'line-through';
  }
  if ($pStyle->getItalic()) {
    $css['font-style'] = 'italic';
  }

  $css['color']   = '#' . $pStyle->getColor()->getRGB();
  $css['font-family'] = '\'' . $pStyle->getName() . '\'';
  $css['font-size'] = $pStyle->getSize() . 'pt';

  // Return
  return $css;
}

/**
 *
 * @param type $sc
 * @param type $key
 * @param type $style
 * @return type 
 */
function _sheetnode_phpexcel_insert_hash(&$sc, $key, $style) {
  $hash = @$sc[$key.'hash'][$style];
  if (is_null($hash)) {
    /*
    $hash = count($sc[$key.'s']) + 1;
    $sc[$key.'s'][$hash] = $style;
    $sc[$key.'hash'][$style] = $hash;
    */
  }
  return $hash;
}

/**
 *
 * @param type $font
 * @return type 
 */
function _sheetnode_phpexcel_get_font($font) {
  return ($font->getItalic() ? 'italic' : 'normal') .' '.
         ($font->getBold() ? 'bold' : 'normal') .' '.
         ($font->getSize() .'pt') .' '.
         ($font->getName());
}

/**
 *
 * @param type $border
 * @return type 
 */
function _sheetnode_phpexcel_get_border($border) {
  $borderstyles = array(
    PHPExcel_Style_Border::BORDER_NONE => FALSE,
    PHPExcel_Style_Border::BORDER_THIN => 'thin solid', 
    PHPExcel_Style_Border::BORDER_MEDIUM => 'medium solid', 
    PHPExcel_Style_Border::BORDER_DASHED => 'thin dashed', 
    PHPExcel_Style_Border::BORDER_DOTTED => 'thin dotted', 
    PHPExcel_Style_Border::BORDER_THICK => 'thick solid', 
    PHPExcel_Style_Border::BORDER_DOUBLE => 'thin double', 
    PHPExcel_Style_Border::BORDER_HAIR => 'thin dotted',  // TODO: is it?
    PHPExcel_Style_Border::BORDER_MEDIUMDASHED => 'medium dashed', 
    PHPExcel_Style_Border::BORDER_DASHDOT => 'thin dashed', // TODO: is it?
    PHPExcel_Style_Border::BORDER_MEDIUMDASHDOT => 'medium dashed', //TODO: is it?
    PHPExcel_Style_Border::BORDER_DASHDOTDOT => 'thin dashed', // TODO: is it?
    PHPExcel_Style_Border::BORDER_MEDIUMDASHDOTDOT => 'medium dashed', // TODO: is it?
    PHPExcel_Style_Border::BORDER_SLANTDASHDOT => 'thin dashed', // TODO: is it?
  );
  $borderstyle = $borderstyles[$border->getBorderStyle()];
  if (!$borderstyle) return FALSE;
  $bordercolor = _sheetnode_phpexcel_get_color($border->getColor());
  return $borderstyle .' '. $bordercolor;
}

/**
 *
 * @param type $style
 * @return string 
 */
function _sheetnode_phpexcel_get_cellformat($style) {
  $cellformats = array(
    PHPExcel_Style_Alignment::HORIZONTAL_GENERAL => FALSE,
    PHPExcel_Style_Alignment::HORIZONTAL_LEFT => 'left',
    PHPExcel_Style_Alignment::HORIZONTAL_CENTER => 'center',
    PHPExcel_Style_Alignment::HORIZONTAL_RIGHT => 'right',
    PHPExcel_Style_Alignment::HORIZONTAL_JUSTIFY => 'justify',
    PHPExcel_Style_Alignment::HORIZONTAL_CENTER_CONTINUOUS => 'center', // TODO: is it?
  );
  return $cellformats[$style->getAlignment()->getHorizontal()];
}

/**
 *
 * @param type $style
 * @return type 
 */
function _sheetnode_phpexcel_get_layout($style) {
  $valignments = array(
    PHPExcel_Style_Alignment::VERTICAL_TOP => 'top',
    PHPExcel_Style_Alignment::VERTICAL_CENTER => 'middle',
    PHPExcel_Style_Alignment::VERTICAL_BOTTOM => 'bottom', 
    PHPExcel_Style_Alignment::VERTICAL_JUSTIFY => 'middle', // TODO: is it?
  );
  $valignment = $valignments[$style->getAlignment()->getVertical()];
  $padding_left = $style->getAlignment()->getIndent();
  $padding_left = empty($padding_left) ? '*' : PHPExcel_Shared_Drawing::pointsToPixels($padding_left) . 'px';
  return 'padding:* * * '. $padding_left .';vertical-align:'. $valignment .';';
}

/**
 *
 * @param type $numberformat
 * @return type 
 */
function _sheetnode_phpexcel_get_valueformat($numberformat) {
  $valueformat = $numberformat->getFormatCode();
  if (strtolower($valueformat) == 'general') return FALSE;
  $valueformat = str_replace(';@', '', $valueformat);
  return $valueformat;
}

/**
 *
 * @param type $color
 * @return type 
 */
function _sheetnode_phpexcel_get_color($color) {
  $rgb = $color->getRGB();
  return 'rgb(' . hexdec(substr($rgb, 0, 2)) .','. hexdec(substr($rgb, 2, 2)) .','. hexdec(substr($rgb, 4, 2)) .')';
}

/**
 * Determine whether the $title matches any string in the array of 
 * $filter_sheet_names (case insensitive)
 * 
 * @param type $title
 * @param type $filter_sheet_names
 * @return boolean 
 */
function _sheetnode_phpexcel_title_matches($title, $filter_sheet_names) {
  $match = false;
  $acount = count($filter_sheet_names);

  for ($i = 0; $i < $acount; $i++)    {
    $atitle = strtolower($filter_sheet_names[$i]);
    if ($atitle == strtolower($title))  {
      $match = true;
      break;
    }
  }

  return $match;
}
        
