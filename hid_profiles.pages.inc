<?php

/**
 * Page callback for Humanitarian ID user debug page.
 */
function hid_profiles_user_debug($user) {
  $output = 'No Humanitarian ID Profiles data is found for this user.';

  // Get the Humanitarian ID Profiles API _id value for the user
  $cid = db_query("SELECT cid FROM {hid_profiles} WHERE uid = :uid ", array(':uid' => $user->uid))->fetchField();

  if (!empty($cid)) {
    $data = _hid_profiles_get_user_by_cid($cid);

    $output = "<p>User ID: $cid</p>";
    $output .= '<pre>' . var_export($data, TRUE) . '</pre>';
  }

  return array(
    '#markup' => $output,
  );
}

/**
 * Admin form for managing the system variables associated with this module.
 */
function hid_profiles_settings_form($form, &$form_state) {
  $form['hid_profiles_api_endpoint'] = array(
    '#type' => 'textfield',
    '#title' => t('API endpoint'),
    '#description' => t('The base URL for endpoints of the Humanitarian ID Profiles API.'),
    '#default_value' => variable_get('hid_profiles_api_endpoint', NULL),
    '#required' => TRUE,
  );
  $form['hid_profiles_api_key'] = array(
    '#type' => 'textfield',
    '#title' => t('API key'),
    '#description' => t('The machine name of this application as defined in the Humanitarian ID Profiles API.'),
    '#default_value' => variable_get('hid_profiles_api_key', NULL),
    '#required' => TRUE,
  );
  $form['hid_profiles_api_secret'] = array(
    '#type' => 'textfield',
    '#title' => t('API secret'),
    '#description' => t('The secret key provided by the Humanitarian ID Profiles API.'),
    '#default_value' => variable_get('hid_profiles_api_secret', NULL),
    '#required' => TRUE,
  );
  return system_settings_form($form);
}


/**
 * Callback function to return an autocomplete JSON object of matching
 * Organizations.
 *
 * @param string $org_names
 *  A keyword for searching Organization names.
 *
 * @return
 *  JSON output of the matching Organizations array.
 */
function hid_organizations_autocomplete($org_names) {
  $matches = array();

  // Get the vocabulary for the Organizations to limit the term query by VID.
  $vocabulary = taxonomy_vocabulary_machine_name_load('hr_organization');

  // Determine the max number of Organizations to return per request.
  $limit = variable_get('hid_organizations_autocomplete_limit', 30);

  // Limit the query to "begins with" or "contains"
  $contains = variable_get('hid_organizations_autocomplete_true', TRUE);
  $contains_delimiter = ($contains) ? '%' : '';

  // Search for hr_organization taxonomy terms, where the term name or
  // organization field value match the given search string.
  $query = db_select('taxonomy_term_data', 't');
  $query->join('field_data_field_acronym', 'fdfa', 't.tid = fdfa.entity_id');
  $query
    ->fields('t')
    ->fields('fdfa')
    ->condition('t.vid', $vocabulary->vid)
    ->condition('fdfa.entity_type', 'taxonomy_term');

  $or = db_or();
  $or->condition('t.name', $contains_delimiter . db_like($org_names) . '%', 'LIKE');
  $or->condition('fdfa.field_acronym_value', $contains_delimiter . db_like($org_names) . '%', 'LIKE');

  $return = $query
    ->condition($or)
    ->range(0, $limit)
    ->execute();

  // add matches to $matches
  foreach ($return as $row) {
    $display_name = $row->name;
    if (!empty($row->field_acronym_value)) {
      $display_name .= ' (' . $row->field_acronym_value . ')';
    }
    $matches['hrinfo_org_' . $row->tid] = $display_name;
  }

  // return for JS
  drupal_add_http_header('Access-Control-Allow-Origin', '*');
  drupal_add_http_header('Access-Control-Allow-Methods', 'GET');
  drupal_json_output($matches);
  drupal_exit();
}

/**
 * Callback function to return an autocomplete JSON object of matching
 * Locations.
 *
 * @return
 *  JSON output of all countries for Locations.
 */
function hid_locations_countries() {
  $countries = array();

  // Get the vocabulary for the Locations to limit the term query by VID.
  $vocabulary = taxonomy_vocabulary_machine_name_load('hr_location');

  // Get the top-level terms for the Location vocabulary.
  $terms = taxonomy_get_tree($vocabulary->vid, 0, 1);

  foreach ($terms as $term) {
    $countries['hrinfo_loc_' . $term->tid] = $term->name;
  }

  // return for JS
  drupal_add_http_header('Access-Control-Allow-Origin', '*');
  drupal_add_http_header('Access-Control-Allow-Methods', 'GET');
  drupal_json_output($countries);
  drupal_exit();
}

/**
 * Callback function to return an autocomplete JSON object of matching
 * Locations.
 *
 * @return
 *  JSON output of all countries for Locations.
 */
function hid_location_country($country) {
  // country.country_id
  // country.name
  // country.regions
  // country.regions.region_id.name
  // country.regions.region_id.cities
  // country.regions.region_id.cities.city_id
  $countries = array(
    'country_id'  => $country->tid,
    'name'        => $country->name,
    'regions'     => array(),
  );

  // Get the vocabulary for the Locations to limit the term query by VID.
  $vocabulary = taxonomy_vocabulary_machine_name_load('hr_location');

  // Get the next 2 levels of terms for the Country term as the parent.
  $terms = taxonomy_get_tree($vocabulary->vid, $country->tid, 2);

  foreach ($terms as $term) {
    if (in_array($country->tid, $term->parents)) {
      if (!isset($countries['regions']['hrinfo_loc_' . $tid])) {
        $countries['regions']['hrinfo_loc_' . $term->tid] = array(
          'name' => $term->name,
          'cities' => array(),
        );
      }
    }
    else {
      if (!isset($countries['regions']['hrinfo_loc_' . $term->parents[0]])) {
        $countries['regions']['hrinfo_loc_' . $term->parents[0]] = array(
          'name' => $term->name,
          'cities' => array(),
        );
      }
      $countries['regions']['hrinfo_loc_' . $term->parents[0]]['cities']['hrinfo_loc_' . $term->tid] = $term->name;
    }
  }

  // return for JS
  drupal_add_http_header('Access-Control-Allow-Origin', '*');
  drupal_add_http_header('Access-Control-Allow-Methods', 'GET');
  drupal_json_output($countries);
  drupal_exit();
}

/**
 * Callback function to display the list of local contacts for a node
 *
 * @param integer Operation Id
 *
 */
function hid_profiles_local_contacts($operation) {
  // Get contacts
  $params = array(
    'locationId' => 'hrinfo:'.$operation->nid,
    'type' => 'local',
    'status' => 1,
  );
  $response = _hid_profiles_get_contacts($params);
  if ($response != FALSE) {
    $contacts = $response->contacts;
    if (count($contacts)) {
      $headers = array(
        'Clusters',
        'First name',
        'Last name',
        'Email',
        'Job Title',
        'Organization',
        'Location'
      );
      $rows = array();
      foreach ($contacts as $contact) {
        
        $cell = array('', $contact->nameGiven, $contact->nameFamily, '', $contact->jobtitle, '', '');
        $rows[] = $cell;
      }
      $table = array('header' => $headers, 'rows' => $rows, 'attributes' => array());
      return theme_table($table);
    }
  }
  return 'There are no contacts to display.';
}
