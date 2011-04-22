README.txt
==========

Multilogin module enables registration via social networks and public services.
Contains set of plugins to authenticate users with their Facebook, Twitter, Vkontakte accounts.

COMPATIBILITY NOTES
-----------------------------
incompatibile with facebook connect, twitter and vk_openapi

INSTALLATION
-----------------------------
see INSTALL.txt

DEPENDENCIES
-----------------------------
* Libraries API module <http://drupal.org/project/libraries>
* oAuh module for Twitter

AUTHORS/MAINTAINERS
-----------------------------
		Danil Semelenov
		Klera Vilenskaya

DEVELOPERS
-----------------------------
To extend multilogin functionality you can use:

* hook_multilogin_create_user()
it is caled when new user is about to register
implementation of this hook allows to alter user object - add custom fields, etc.

@param $plugin
    plugin name  (==  external system name) - facebook, twitter or another
@param $func
   called function name  - currently only "create_user" is avaliable
@param $array
    array to construct user object, contains something like
	$array = array(
		'name' =>   $me['name'],
		'mail' => $me['email'],
		'init' => 'facebook',
		'pass' => user_password(),
		'status' => variable_get('user_register', 1) == 1,
		'multilogin' => array('facebook' => $me),
	);
where $me is external system profile information
@param $ext_profile
external system profile information - facebook profile, twitter user information

Example:
	function mymodule_multilogin_create_user($plugin, $func, &$array , &$ext_profile ) {
		if($func == 'create_user') {
			if(($plugin == 'facebook') or ($plugin == 'vkontakte')){
			     // save  facebook\ vkontakte name to profile
					$array['profile_name'] = $ext_profile['first_name'];
				$array['profile_lastname'] = $ext_profile['last_name'];
			}
		}
	}
