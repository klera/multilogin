<?php
// fork of  twitter_post.module,v 1.1.2.1 2010/01/25 05:09:05 walkah
// changes: oauth support, multilogin module support, functions from twitter.module needed but not presented in  twitter_post.module

/**
 * Settings form callback
 */
function  multilogin_twitter_post_admin_settings() {
  $form['multilogin_twitter_post_types'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Node types'),
    '#options' => node_get_types('names'),
    '#default_value' => variable_get('twitter_post_types', array('story' => 'story', 'blog' => 'blog')),
  );

  $form[' multilogin_twitter_post_default_format'] = array(
    '#type' => 'textfield',
    '#title' => t('Default format string'),
    '#maxlength' => 140,
    '#description' => t('The given text will be posted to twitter.com. You can use !url, !url-alias, !tinyurl, !title, and !user as replacement text.'),
    '#default_value' => variable_get('twitter_post_default_format', 'New post: !title !tinyurl'),
  );

  return system_settings_form($form);
}
