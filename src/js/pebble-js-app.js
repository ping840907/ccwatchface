const Clay = require('pebble-clay');
const clayConfig = require('./config.json');

// Function to manipulate the config before opening
function manipulateConfig() {
  const watchInfo = Pebble.getActiveWatchInfo();

  // Check if the platform is black and white
  if (watchInfo && (watchInfo.platform === 'aplite' || watchInfo.platform === 'diorite')) {
    // Find the section containing the theme toggle
    const colorsSection = clayConfig.find(item => item.type === 'section' && item.items[0].defaultValue === 'Colors');
    if (colorsSection) {
      // Find the index of the theme toggle
      const toggleIndex = colorsSection.items.findIndex(item => item.messageKey === 'KEY_THEME_IS_DARK');
      // If found, remove it
      if (toggleIndex > -1) {
        colorsSection.items.splice(toggleIndex, 1);
      }
    }
  }
}

const clay = new Clay(clayConfig, manipulateConfig);