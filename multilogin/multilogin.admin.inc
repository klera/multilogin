<?php
/**
 *  settings form.
 */
function multilogin_settings_form() { // TODO: put settings of each provider to its plugin
  $prefix = 'multilogin';
  $multilogin_config = variable_get($prefix, array());
  //dsm($multilogin_config);
   global $multilogin;

   $form = array();

  $form['fastreg']  = array(
    '#title' => t('Fast registration mode'),
    '#type' => 'checkbox',
    '#parents' => array($prefix, 'fastreg' ),
    '#default_value' => $multilogin_config['fastreg'],
    '#description' => t('To enable registration without entering login or email'),
  );

  $roles= user_roles();
 	array_unshift($roles, '');

	$form["multilogin_noemail_rid"]= array(
	'#type'		=> "select",
	'#parents' => array($prefix, 'multilogin_noemail_rid' ),
	'#title'	=> t('Role for users without email'),
	'#options'=>	$roles,
	"#default_value"=> $multilogin_config['multilogin_noemail_rid'], //variable_get('multilogin_noemail_rid', '')
	);

   $directory = drupal_get_path('module', 'multilogin').'/providers';
   $files =  file_scan_directory($directory, '\.inc', array('.', '..', 'CVS'), 0, TRUE, 'name', 0) ;

   if (count($files)) {
     foreach ($files as $app_name => $app) {   //$app->name
        include "providers/$app_name/$app_name.inc";
       $function = "multilogin_{$app_name}_settings_form";
       if (function_exists($function)) {
         call_user_func($function, &$form, $form_state, $multilogin_config, $prefix);
       }
     }
   }
  // dsm($form);

   return system_settings_form($form);
}

