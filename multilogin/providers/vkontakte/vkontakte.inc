<?php

/**
 * @file
 * Plugin for  multilogin.module, enables user registration via twitter
 TODO: rewrite using oAuth
 */


function multilogin_vkontakte_init() {
  global $multilogin;
  $multilogin_config = variable_get('multilogin', array());

  $vkontakte_inc = libraries_get_path('vkapi.class.php');

  if (!file_exists($vkontakte_inc)) {
    drupal_set_message(t('Error loading Vkontakte class'), 'error');
  }
  else {
    include_once( $vkontakte_inc);

    $multilogin['vkontakte']['VK'] = new vkapi($multilogin_config['vkontakte']['app_id'], $multilogin_config['vkontakte']['app_secret']);
    define('APP_ID', $multilogin_config['vkontakte']['app_id']);
    define('APP_SHARED_SECRET', $multilogin_config['vkontakte']['app_secret']);
    $multilogin['vkontakte']['member'] = authOpenAPIMember();
    // dsm($multilogin['vkontakte']['member']);
  }
}

/**
 *  copied from http://vk.com/developers.php?o=-17680044&p=VK.Auth
 */
function authOpenAPIMember() {
  $session = array();
  $member = FALSE;
  $valid_keys = array('expire', 'mid', 'secret', 'sid', 'sig');
  $app_cookie = isset($_COOKIE['vk_app_' . APP_ID]) ? $_COOKIE['vk_app_' . APP_ID] : null;
  if ($app_cookie) {
    $session_data = explode('&', $app_cookie, 10);
    foreach ($session_data as $pair) {
      list($key, $value) = explode('=', $pair, 2);
      if (empty($key) || empty($value) || !in_array($key, $valid_keys)) {
        continue;
      }
      $session[$key] = $value;
    }
    foreach ($valid_keys as $key) {
      if (!isset($session[$key])) {
        return $member;
      }
    }
    ksort($session);

    $sign = '';
    foreach ($session as $key => $value) {
      if ($key != 'sig') {
        $sign .= ($key . '=' . $value);
      }
    }
    $sign .= APP_SHARED_SECRET;
    $sign = md5($sign);
    if ($session['sig'] == $sign && $session['expire'] > time()) {
      $member = array(
        'id' => intval($session['mid']),
        'secret' => $session['secret'],
        'sid' => $session['sid']
      );
    }
  }
  return $member;
}

function multilogin_vkontakte_preprocess_page(&$variables) { //    this is coz drupal wants to add js hosted ourselves js. put module to the end of the chain to not allow othher module override $variables['scripts'] by value from drupal_add_js()
  $variables['scripts'] .= '<script type="text/javascript" src="http://vkontakte.ru/js/api/openapi.js"></script>' . "\n"; // dsm($variables);
}

function multilogin_vkontakte_login_form_alter(&$form, &$form_state, $form_id) {
  global $multilogin;

  $multilogin_config = variable_get('multilogin', array());
  $member = $multilogin['vkontakte']['member'];

  if ($form_id != 'user_register') {
     if ($member) return;

    $form['#suffix'] .= '<div id="login_button" onclick="VK.Auth.login(authInfo);"></div>';
    _multilogin_add_css('vkontakte');

    $js = <<<JS
VK.UI.button('login_button');
VK.init({
  apiId: %s,
  status: true
});
function authInfo(response) {
  if (response.status == 'connected') {
    location = Drupal.settings.basePath + 'multilogin/authenticate/vkontakte';
  }
//   if (!response.session) {
//     VK.not_authorized = true;
//   }
//   else if (VK.not_authorized) {
//     location = Drupal.settings.basePath + 'multilogin/authenticate/vkontakte';
//   }
}
// VK.Auth.getLoginStatus(authInfo);
JS;
    $js = sprintf($js, $multilogin_config['vkontakte']['app_id']);
    drupal_add_js($js, 'inline', 'footer');
  }
  else {
    if (!$member || !isset($_SESSION['vkontakte']['values'])) {
      return;
    }

    drupal_set_message(t('Please complete the registration by filling out the form below. If you already have an account, you can <a href="@login">log in</a> now and add your OpenID under "My account".', array('@login' => url('user/login'))), 'warning');

    $form['name']['#default_value'] = $_SESSION['vkontakte']['values']['name'];
    $form['mail']['#default_value'] = '';

    if (!variable_get('user_email_verification', TRUE)) {
      $form['pass']['#type'] = 'hidden';
      $form['pass']['#value'] = user_password();
    }
		 $form['vkontakte_display'] = array(
		      '#type' => 'item',
		      '#title' => t('Your Vkontakte profile'),
		      '#description' => t('This profile will be added to your account after registration'),
		      '#value' => 'http://vkontakte.ru/' . $_SESSION['vkontakte']['values']['domain'],
		    );
  }
}

function multilogin_vkontakte_authentication_page() {
  global $multilogin, $base_path;

  $dst = referer_uri();
  $dst = parse_url( $dst); // url only,, no relative path here!
  $redirect = trim( $dst['path'], '/');


  $action = arg(3);
  if ($action) {
    switch ($action) {
      case 'logout': // dsm('multilogin_vkontakte_authentication_page with logout - line 137');
        /* multilogin_vkontakte_preprocess_page($variables);
         $html = <<<HTML
         <html>
         <head>
         %s
         </head>
         <body>
         <script type="text/javascript">
         VK.init({
         apiId: %s
         });
         VK.Auth.logout(function(response) {
         location = '%s';
         });
         </script>
         <p>Wait...</p>
         </body>
         </html>
         HTML;
         $html = sprintf($html, $variables['scripts'], APP_ID, $base_path);
         print $html;

         return;*/
        break;
    }
  }

  $member = $multilogin['vkontakte']['member'];

  $VK = $multilogin['vkontakte']['VK'];
  $result = $VK->api('getProfiles', array(
      'uids' => $member['id'],
      'fields' => 'first_name,nickname,domain,sex,bdate,photo_rec,city,country,photo_big,photo_medium,photo_big,contacts,university,university_name,faculty,faculty_name,graduation',
    ));
  /* http://vkontakte.ru/developers.php?o=-1&p=%D0%9E%D0%BF%D0%B8%D1%81%D0%B0%D0%BD%D0%B8%D0%B5+%D0%BF%D0%BE%D0%BB%D0%B5%D0%B9+%D0%BF%D0%B0%D1%80%D0%B0%D0%BC%D0%B5%D1%82%D1%80%D0%B0+fields
   */
  $profile = $result['response'][0];

  $account = user_external_load('vkontakte::' . $member['id']); //  $account = user_external_load($member['id']);

  if ($account) {
    // we have this user
    // update info
    $edit['multilogin'] = $account->multilogin;
    $edit['multilogin']['vkontakte'] = $profile;
    user_save($account, $edit);
    // login
    user_external_login($account);
  }
  else {   // todo: add fast reg
    $user_array = multilogin_vkontakte_create_user($profile);
    $account = user_save(NULL, $user_array);
    // map  to user
    _multilogin_vkontakte_map_user($account);
    // auth
    user_external_login($account);
    // $redirect = 'user/register';
  }

  if (!isset($redirect)) {
    $redirect = '';
  }
  drupal_goto($redirect);
}

function multilogin_vkontakte_create_user($profile) {
  $account_name = multilogin_get_free_username($profile['first_name'] . ' ' . $profile['last_name']);
  $array = array(
		          'name' => $account_name,
		          'mail' => '',
		          'init' => 'vkontakte',
		          'pass' => user_password(),
		          'status' => variable_get('user_register', 1) == 1,
		          'multilogin' => array('vkontakte' => $profile),
		        );

	multilogin_module_invoke('vkontakte', 'create_user', $array , $profile );

  return $array;
}


function multilogin_vkontakte_user($op, &$edit, &$account, $category = NULL) {
  global $multilogin, $base_url, $base_path;

  switch ($op) {
    case 'insert':
      if (isset($_SESSION['vkontakte'])) {
        // map_user
        _multilogin_vkontakte_map_user($account);
        unset($_SESSION['vkontakte']);
      }

      break;

    case 'logout':
      if (isset($_COOKIE['vk_app_' . APP_ID])) {
        unset($_COOKIE['vk_app_' . APP_ID]);
//   $_REQUEST['destination'] = 'multilogin/authenticate/vkontakte/logout';
      }

      break;
  }
}

function _multilogin_vkontakte_map_user($account) {
  global $multilogin;

  $member = $multilogin['vkontakte']['member'];
  user_set_authmaps($account, array('authname_vkontakte' => 'vkontakte::' . $member['id']));
}


function multilogin_vkontakte_get_userpic( $account) {
  if ($account->multilogin['twitter']) {
    return $multilogin['twitter']['profile_image_url'];
  }
}


function  multilogin_vkontakte_settings_form(&$form, $form_state, $multilogin_config, $prefix) {
 $form['vkontakte'] = array(
    '#title' => 'VKontakte',
    '#tree' => TRUE,
    '#parents' => array($prefix, 'vkontakte'),
    '#type' => 'fieldset',
    '#description' => t('To enable sign-in using vkontakte, you have to <a href="@url">register your application</a> and add the provided keys here.', array('@url' => 'http://vkontakte.ru/apps.php?act=add&site=1')),

  );
  $form['vkontakte']['enabled'] = array(
    '#title' => t('enabled'),
    '#type' => 'checkbox',
    '#default_value' => $multilogin_config['vkontakte']['enabled'],
  );
  $form['vkontakte']['app_id'] = array(
    '#title' => t('ID приложения / application ID'),
    '#type' => 'textfield',
    '#default_value' => $multilogin_config['vkontakte']['app_id'],
  );
  $form['vkontakte']['app_secret'] = array(
    '#title' => t('Защищенный ключ / protected key'),
    '#type' => 'textfield',
    '#default_value' => $multilogin_config['vkontakte']['app_secret'],
  );
}
