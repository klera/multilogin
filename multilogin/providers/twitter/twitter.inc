<?php
/**
 * @file
 * Plugin for  multilogin.module, enables user registration via twitter
 */

/**
 * Internal init function
 */
function multilogin_twitter_init() {
  global $multilogin;
  global $user;
  $multilogin_config = variable_get('multilogin', array());

  if (!module_exists('oauth')) {
    drupal_set_message('oauth module not found. Twitter authorization is inactive', 'error');
    return;
  }

  $twitter_inc = libraries_get_path('twitter.lib.php');

  if (!file_exists($twitter_inc)) {
    drupal_set_message('Error loading  twitter library.', 'error');
    return;
  }

  include_once($twitter_inc);
  module_load_include('lib.php', 'oauth');

  //todo : call constructor with user params if exist
  $values = array();
  $values = $user->multilogin['twitter'];

  $account = new TwitterUser($values);
  $account->set_auth($values);
  //	 $auth = $account->get_auth();

  if ( $values['oauth_token'] && $values['oauth_token_secret']) {
    $twitter =  new TwitterOAuth($multilogin_config['twitter']['twitter_consumer_key'], $multilogin_config['twitter']['twitter_consumer_secret'], $values['oauth_token'], $values['oauth_token_secret']);
  }
  else {
    $twitter =  new TwitterOAuth($multilogin_config['twitter']['twitter_consumer_key'], $multilogin_config['twitter']['twitter_consumer_secret']);
  }
  // dsm($twitter);
  $multilogin['twitter']['twitter'] = $twitter;
  $multilogin['twitter']['user'] = $user->multilogin['twitter'];
}

/**
 * Internal function to alter login\register form
 */
function multilogin_twitter_login_form_alter(&$form, &$form_state, $form_id) {
  global $language, $multilogin;
  $twitter_user = $multilogin['twitter']['user'];
//  $twitter = $multilogin['twitter']['twitter'];

  if ($form_id != 'user_register') {
    if ($twitter_user) {
      return;
    }

    $img = drupal_get_path('module', 'multilogin') . '/providers/twitter/' . 'twitter.png';
    $form['twitter_button'] = array("#type" => "markup", "#value" => l(theme('image', $img, t('Sign in with Twitter')), 'multilogin/authenticate/twitter/redirect', array('html' => TRUE )));
		//  _multilogin_add_css('twitter');
  }
  else {
    if ( isset($_SESSION['twitter'])) {
      $form['name']['#default_value'] = $_SESSION['twitter']['screen_name'];
      $form['account']['name']['#default_value'] = $_SESSION['twitter']['screen_name'];
      $form['auth_twitter'] = array('#type' => 'hidden', '#value' => $_SESSION['twitter']['id']);
      $form['twitter_display'] = array(
				'#type' => 'item',
				'#title' => t('Your Twitter profile'),
				'#description' => t('This profile will be linked to your account after registration'),
				'#value' => $_SESSION['twitter']['screen_name'],
			);
      drupal_set_message(t('Please complete the registration by filling out the form below. If you already have an account, you can <a href="@login">log in</a> now and add your profile under "My account".', array('@login' => url('user/login'))), 'warning');
    }

    if (!variable_get('user_email_verification', TRUE)) {
      $form['pass']['#type'] = 'hidden';
      $form['pass']['#value'] = user_password();
    }
    // additional  TODO: move somewhere else
    $form['Personal information']['profile_name']['#default_value'] = $_SESSION['twitter']['name'];
  }
}

/**
 * Auth - called when user logs in\register or adds fb profile to his account
 */
function multilogin_twitter_authentication_page() {
  global $multilogin; //dsm($multilogin);
  if (isset($_SESSION['redir'])) {
    $redirect = $_SESSION['redir'];
  }

  $twitter_user = $multilogin['twitter']['user'];
  $account = user_external_load("twitter::$twitter_user");  // $account = user_external_load($twitter_user); // dsm($account);
  if ($account) {
    // user has linked to acc
    user_external_login($account);
    drupal_goto($redirect);
  }
  else {
    $dst = $_GET['destination'];
    if (!$dst) {
      $dst = referer_uri();
      $dst = parse_url( $dst); // url only, avoid relative path here
      $dst = trim( $dst['path'], '/');
    }

    $action = arg(3); // dsm($action);
    if ($action == 'redirect') {
      $twitter =  $multilogin['twitter']['twitter'];
      $token = $twitter->get_request_token();

      $_SESSION['twitter_oauth']['token'] = $token;
      $_SESSION['twitter_oauth']['destination'] = url('multilogin/authenticate/twitter/in', array('absolute' => true));
      $_SESSION['twitter_oauth']['signin'] = TRUE;
      $_SESSION['redir'] = $dst;
      //dsm($twitter->get_authenticate_url($token));
      drupal_goto($twitter->get_authenticate_url($token));
    }

    if ($action == 'in') {
      $sucsess = false;

      $statevalues = $_GET;
      if ($statevalues['denied'] ) {
        $sucsess = false;
        drupal_set_message( t('Twitter authentification failed.'), 'error'); // user chancelled
      }
      else {
        $statetwitter_oauth = $_SESSION['twitter_oauth'];

        $token = $statetwitter_oauth['token'];
        if (!is_array($token) ) {
          drupal_set_message( t('Invalid Twitter OAuth request'), 'error');
          $sucsess = false;
        }

        if ($token['oauth_token'] != $statevalues['oauth_token']) {
          drupal_set_message( t('Invalid Twitter OAuth token.'), 'error');
          $sucsess = false;
        }

        if ($token['oauth_token'] === $statevalues['oauth_token']) {
          $sucsess = true;
        }
      }
      unset($_SESSION['twitter_oauth']);

      if ($sucsess) {
        $multilogin_config = variable_get('multilogin', array());

        $twitter =  new TwitterOAuth($multilogin_config['twitter']['twitter_consumer_key'], $multilogin_config['twitter']['twitter_consumer_secret'],   $token['oauth_token'], $token['oauth_token_secret']);
        $response = $twitter->get_access_token();
        // dsm($response);
        $twitter =  new TwitterOAuth($multilogin_config['twitter']['twitter_consumer_key'], $multilogin_config['twitter']['twitter_consumer_secret'], $response['oauth_token'], $response['oauth_token_secret']);
        $taccount = $twitter->users_show($response['screen_name']);
        $taccount->set_auth($response);

        $twitter_user = $taccount->id;
        $multilogin['twitter']['user'] = $twitter_user;

        $account = user_external_load("twitter::$twitter_user");

        global $user;
        if ($user->uid) { // logged in user adds twi account
          if (($account ) && ($account->uid != $user->uid)) {
            drupal_set_message( t('this Twitter account is already registred as another user. Adding account failed.'), 'error'); // user chancelled
            // todo: suggest merge accounts
          }
          else {
            _multilogin_twitter_map_user($user);
            $user_array = multilogin_twitter_create_user($taccount);
            $user->multilogin['twitter'] = $user_array['multilogin']['twitter'];
            $acc = user_save($user, array('multilogin' => $user->multilogin));
          }
          $redirect = 'user/' . $user->uid . '/multilogin';
          drupal_goto($redirect);
        }

        if ($account) { // user exists , log him in
          user_external_login($account);
          //refresh userinfo. fresh tokens required for posting
          $user_array = multilogin_twitter_create_user($taccount);

          $account->multilogin['twitter'] = $user_array['multilogin']['twitter'];
          $acc = user_save($account, array('multilogin' => $account->multilogin));
        }
        else { // there is no account for this twi user, try create it
          if ($multilogin_config['fastreg']) {
            $user_array = multilogin_twitter_create_user($taccount);
            $account = user_save(NULL, $user_array);
            _multilogin_twitter_map_user($account);
            // add role coz twi don`t shows email
        		multilogin_set_noemail_rid($account);
            // auth
            user_external_login($account);
          }
          else {
            $_SESSION['twitter'] =  (array) $taccount;
            $redirect = 'user/register';
          }
        }
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
function multilogin_twitter_create_user($taccount) {
  // remove unnecessary fields
  $t_account = array('id' => $taccount->id, 'screen_name' => $taccount->screen_name, 'name' => $taccount->name, 'url' => $taccount->url, 'protected' => $taccount->protected, 'profile_image_url' => $taccount->profile_image_url );
  $t_account += $taccount->get_auth();

  $account_name = multilogin_get_free_username($taccount->screen_name . '@twitter.com');
  $array = array(
		          'name' => $account_name,
		          'mail' => $taccount->screen_name . '',   // not an error, just create user with inwalid mail
		          'init' => 'twitter',
		          'pass' => user_password(),
		          'status' => variable_get('user_register', 1) == 1,
		          'multilogin' => array('twitter' => $t_account),
		        );

	multilogin_module_invoke('twitter', 'create_user', $array , $taccount );

  return $array;
}

/**
 * internal function used to display names
 */
function multilogin_twitter_show_name( $account, $authname = null) {
  if ($account->multilogin['twitter']) {
    return $account->multilogin['twitter']['screen_name'];
  }
}

/**
 * internal plugin implementation of hook_user
 */
function multilogin_twitter_user($op, &$edit, &$account, $category = NULL) {
  global $multilogin, $base_url, $base_path;

  switch ($op) {
    case 'insert':
      if (isset($_SESSION['twitter'])) {
        _multilogin_twitter_map_user($account);
        unset($_SESSION['twitter']);
      }
      break;

    case 'logout':
      // $twitter = $multilogin['twitter']['twitter'];
      break;
  }
}

/**
 * maps user account to external profile
 */
function _multilogin_twitter_map_user($account) {
  global $multilogin;
  $twitter_user = $multilogin['twitter']['user'];
  user_set_authmaps($account, array('authname_twitter' => "twitter::$twitter_user"));
}


function multilogin_twitter_get_userpic( $account) {
  if ($account->multilogin['twitter']) {
    return $multilogin['twitter']['profile_image_url'];
  }
}

function  multilogin_twitter_settings_form(&$form, $form_state, $multilogin_config, $prefix) {
  $form['twitter']['desc']  = array(
    '#type' => 'markup',
    '#value' => t('<p>To enable twitter you need oAuth module</p> '),  // todo: disable form if no oAuth
  );
  $form['twitter'] = array(
    '#type' => 'fieldset',
    '#title' => t('Twitter'),
    '#tree' => TRUE,
    '#parents' => array($prefix, 'twitter'),
    '#access' => module_exists('oauth'),
    '#description' => t('To enable OAuth based access for twitter, you have to <a href="@url">register your application</a> with twitter and add the provided keys here.', array('@url' => 'https://twitter.com/apps/new')),
  );
  $form['twitter']['enabled'] = array(
    '#title' => t('enabled'),
    '#type' => 'checkbox',
    '#default_value' => $multilogin_config['twitter']['enabled'],
  );
  $form['twitter']['callback_url'] = array(
    '#type' => 'item',
    '#title' => t('Callback URL'),
    '#value' =>  url('/multilogin/authenticate/twitter/in', array('absolute' => TRUE)),
  );
  $form['twitter']['twitter_consumer_key'] = array(
    '#type' => 'textfield',
    '#title' => t('OAuth Consumer key'),
    '#default_value' => $multilogin_config['twitter']['twitter_consumer_key'],
  );
  $form['twitter']['twitter_consumer_secret'] = array(
    '#type' => 'textfield',
    '#title' => t('OAuth Consumer secret'),
    '#default_value' => $multilogin_config['twitter']['twitter_consumer_secret'],
  );
}
