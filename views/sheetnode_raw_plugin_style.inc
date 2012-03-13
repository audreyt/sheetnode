<?php

class sheetnode_raw_plugin_style extends sheetnode_feed_plugin_style {
  function init(&$view, &$display, $options = NULL) {
    parent::init($view, $display, $options = NULL);
    $this->feed_image = drupal_get_path('module', 'sheetnode') . '/images/socialtext.png';
  }

  function render() {
    $this->inherit_options();
    header('Content-type: text/plain');
    return socialcalc_save(parent::render_sheet());
  }
};

