const Clay = require('pebble-clay');
const clayConfig = require('./config.json');

// Function to manipulate the config before opening
function manipulateConfig() {
  // All platforms can now toggle themes.
}

const clay = new Clay(clayConfig, manipulateConfig);