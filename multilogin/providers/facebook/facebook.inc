<?php
/**
 * @file
 * Plugin for  multilogin.module, enables user registration via facebook
 */

/**
 * Internal init function
 */
function multilogin_facebook_init() {
  global $multilogin;

  $multilogin_config = variable_get('multilogin', array());
  $facebook_inc = libraries_get_path('facebook.php');

  if (!file_exists($facebook_inc)) {
    drupal_set_message('Error loading  Facebook library.', 'error');
  }
  else {
    include_once( $facebook_inc);

    Facebook::$CURL_OPTS[CURLOPT_SSL_VERIFYPEER] = FALSE;
    Facebook::$CURL_OPTS[CURLOPT_SSL_VERIFYHOST] = 2;

    $facebook = new Facebook(array(
      'appId'  => $multilogin_config['facebook']['app_id'],
      'secret' => $multilogin_config['facebook']['app_secret'],
      'cookie' => TRUE,
    ));
    $multilogin['facebook']['facebook'] = $facebook;
    $multilogin['facebook']['user'] = null;
    if ($facebook->getSession()) {
      try {
        $multilogin['facebook']['user'] = $facebook->getUser();
      }
      catch (FacebookApiException $e) {
        error_log($e);
      }
    }
  }
}


function multilogin_facebook_preprocess_page(&$variables) {
  $variables['closure'] = '<div id="fb-root"></div>' . "\n" . $variables['closure'];
}

/**
 * Internal function to alter login\register form
 */
function multilogin_facebook_login_form_alter(&$form, &$form_state, $form_id) {
  global $language, $multilogin;

  $multilogin_config = variable_get('multilogin', array());
  $facebook_user = $multilogin['facebook']['user'];
  $facebook = $multilogin['facebook']['facebook'];

  if ($form_id != 'user_register') {
    if ($facebook_user) {
      return;
    }

    $login_button_text = $multilogin_config['facebook']['login_button_text'];
    if (mb_detect_encoding($login_button_text) == 'ASCII') {
      $login_button_text = t($login_button_text);
    }

    $form['facebook_button'] = array("#type" => "markup", "#value" => sprintf('<div id="facebook-login-button"><fb:login-button perms="email,user_birthday">%s</fb:login-button></div>', $login_button_text));
    _multilogin_add_css('facebook');

    $locale = 'en_US';
    if ($language->language == 'ru') {
      $locale = 'ru_RU';
    }

    $js = <<<JS
window.fbAsyncInit = function() {
  FB.init({
    appId: '%s',
    session: %s,
    status: false,
    cookie: true,
    xfbml: true
  });
  FB.Event.subscribe('auth.login', function() {
    location = Drupal.settings.basePath + 'multilogin/authenticate/facebook';
  });
};
(function() {
  var e = document.createElement('script');
  e.src = document.location.protocol + '//connect.facebook.net/%s/all.js';
  e.async = true;
  document.getElementById('fb-root').appendChild(e);
}());
JS;
    $js = sprintf($js, $facebook->getAppId(), json_encode($facebook->getSession()), $locale);
    drupal_add_js($js, 'inline', 'footer');
  }
  else {
    if (!$facebook_user || !isset($_SESSION['facebook']['values'])) {
      return;
    }

    drupal_set_message(t('Please complete the registration by filling out the form below. If you already have an account, you can <a href="@login">log in</a> now and add your profile under "My account".', array('@login' => url('user/login'))), 'warning');

    $form['name']['#default_value'] = $form['account']['name']['#default_value'] = $_SESSION['facebook']['values']['name'];
    $form['mail']['#default_value'] = $form['account']['mail']['#default_value'] = $_SESSION['facebook']['values']['email'];

    if (!variable_get('user_email_verification', TRUE)) {
      $form['pass']['#type'] = 'hidden';
      $form['pass']['#value'] = user_password();
    }
    $form['facebook_display'] = array(
      '#type' => 'item',
      '#title' => t('Your Facebook profile'),
      '#description' => t('This profile will be linked to your account after registration'),
      '#value' => $_SESSION['facebook']['values']['link'],
    );
    // additional
    $form['Personal information']['profile_name']['#default_value'] = $_SESSION['facebook']['values']['name'];
  }
}

/**
 * Auth - called when user logs in\register or adds fb profile to his account
 */
function multilogin_facebook_authentication_page() {
  global $multilogin;
  global $user;
  $dst = referer_uri();
  $dst = parse_url( $dst); // url only,, no relative path here!
  $redirect = trim( $dst['path'], '/');

  $facebook_user = $multilogin['facebook']['user'];
  $account = user_external_load('facebook::' . $facebook_user);

  if ($account) {
    if (($user->uid)   && ($account->uid != $user->uid)) {
      drupal_set_message( t('this Facebook account is already registred by another user. Adding account failed.'), 'error'); // user chancelled
      $redirect = 'user/' . $user->uid . '/multilogin';
      drupal_goto($redirect);
    }
    else {
      user_external_login($account);
      drupal_goto($redirect);
    }
  }
  else {
    $facebook = $multilogin['facebook']['facebook'];
    $me = $facebook->api('/me');     // dsm($me);
    $urlname = ltrim(parse_url($me['link'], PHP_URL_PATH), '/');
    if ($urlname == 'profile.php') {
      $name = '';
    }
//  $_SESSION['facebook']['values'] = array_merge($me, array('name' => $name));

    if ($user->uid) {
      _multilogin_facebook_map_user($user);
      $user_array = multilogin_facebook_create_user($me);
      $user->multilogin['facebook'] = $user_array['multilogin']['facebook'];
      $acc = user_save($user, array('multilogin' => $user->multilogin));
      $redirect = 'user/' . $user->uid . '/multilogin';
      drupal_goto($redirect);
    }

    $account = NULL;
    if ($me['email']) {
      $account = user_load(array('mail' => $me['email']));
      if (!$account) {
        $user_array = multilogin_facebook_create_user($me);
        $account = user_save(NULL, $user_array);
        // todo: add serpic
      }
      else {
        $edit['multilogin'] = $account->multilogin;
        $edit['multilogin']['facebook'] = $me   ;
        user_save($account, $edit);
      }
      // now acc with this email exists
      // map fb to user
      _multilogin_facebook_map_user($account);
      // auth
      user_external_login($account);
    }
    else {
      $multilogin_config = variable_get('multilogin', array());
      if ($multilogin_config['fastreg']) {
        $user_array = multilogin_facebook_create_user($me);
        $account = user_save(NULL, $user_array); //    dsm($account);
        _multilogin_facebook_map_user($account);
        // add role coz no mail
        multilogin_set_noemail_rid($account);
        // auth
        user_external_login($account);
      }
      else { // $_SESSION['facebook'] =  (array)$taccount;
        $redirect = 'user/register';
      }

    }
  }

  if (!isset($redirect)) {
    $redirect = '';
  }
  drupal_goto($redirect);
}

/**
 * Constructs user object
 */
function multilogin_facebook_create_user($me) {
	$account_name = multilogin_get_free_username($me['name']);

  $array = array(
		          'name' => $account_name, // $me['name'],
		          'mail' => $me['email'],
		          'init' => 'facebook', //$me['email'],
		          'pass' => user_password(),
		          'status' => variable_get('user_register', 1) == 1,
		          'multilogin' => array('facebook' => $me),
		        );

// call hook_multilogin_create_user
	multilogin_module_invoke('facebook', 'create_user', $array , $me );

  return $array;
}

/**
 * internal plugin implementation of hook_user
 */
function multilogin_facebook_user($op, &$edit, &$account, $category = NULL) {
  global $multilogin, $base_url, $base_path;

  switch ($op) {
    case 'insert':
      if (isset($_SESSION['facebook'])) {
        // map
        _multilogin_facebook_map_user($account);
        unset($_SESSION['facebook']);
      }

      break;

    case 'logout':
      $facebook = $multilogin['facebook']['facebook'];
      if ($facebook->setSession()) {
        $_REQUEST['destination'] = $facebook->getLogoutUrl(array('next' => "$base_url$base_path"));
      }

      if ( $facebook->useCookieSupport()) {
        $cookieName = 'fbs_' . $facebook->getAppId();
        $expires = time() - 36000; //!!!

        $value = 'deleted';

        global $cookie_domain;
        $domain = $cookie_domain  ;

        setcookie($cookieName, $value, $expires, '/', $domain);

        // ugly hack for subdomains cause fb class keeps cookie domain private.
        $d = explode('.', $domain);

        if (count($d > 2)) {
          $d = array_reverse($d);

          $domain1 = '.' . $d[1] . '.' . $d[0];
          setcookie($cookieName, $value, $expires, '/', $domain1);
        }
      }

      break;
  }
}

/**
 * maps user account to external profile
 */
function _multilogin_facebook_map_user($account) {
  global $multilogin;

  $facebook_user = $multilogin['facebook']['user'];
  user_set_authmaps($account, array('authname_facebook' => "facebook::$facebook_user"));
}

/**
 * internal function used to display names
 */
function multilogin_facebook_show_name( $account, $authname = null) {
  if ($account->multilogin['facebook']) {
    return $account->multilogin['facebook']['name'];
  }
}

function multilogin_facebook_get_userpic( $account) {
  if ($account->multilogin['facebook']) {
    return 'http://graph.facebook.com/'.$account->multilogin['facebook']['id'].'/picture?type=square';
  }
}

function multilogin_facebook_settings_form(&$form, $form_state, $multilogin_config, $prefix) {
  $form['facebook'] = array(
    '#title' => 'Facebook',
    '#tree' => TRUE,
    '#parents' => array($prefix, 'facebook'),
    '#type' => 'fieldset',
    '#description' => t('To enable sign-in using Facebook, you have to <a href="@url">register your application</a> and add the provided keys here.', array('@url' => 'http://developers.facebook.com/setup/')),
  );
  $form['facebook']['enabled'] = array(
    '#title' => t('enabled'),
    '#type' => 'checkbox',
    '#default_value' => $multilogin_config['facebook']['enabled'],
  );
  $form['facebook']['app_id'] = array(
    '#title' => 'Application ID',
    '#type' => 'textfield',
    '#default_value' => $multilogin_config['facebook']['app_id'],
  );
  $form['facebook']['app_secret'] = array(
    '#title' => 'Application Secret',
    '#type' => 'textfield',
    '#default_value' => $multilogin_config['facebook']['app_secret'],
  );

  $login_button_text = $multilogin_config['facebook']['login_button_text'];

  if (empty($login_button_text)) {
    $login_button_text = 'Connect with Facebook';
  }
  $form['facebook']['login_button_text'] = array(
    '#title' => 'Button text',
    '#type' => 'textfield',
    '#default_value' => $login_button_text,
  );

}

