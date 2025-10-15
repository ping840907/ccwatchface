const Clay = require('pebble-clay');
const clayConfig = require('./config.json');

// Function to manipulate the config before opening
function manipulateConfig() {
  // By default, all options are shown. The logic to hide the theme toggle on
  // B&W devices has been removed, so it is now enabled for all platforms.

  const watchInfo = Pebble.getActiveWatchInfo();

  // Now, specifically hide the accent color picker on B&W devices since
  // it has no effect on those platforms.
  if (watchInfo && (watchInfo.platform === 'aplite' || watchInfo.platform === 'diorite')) {
    // Find the section containing the color picker
    const colorsSection = clayConfig.find(item => item.type === 'section' && item.items[0].defaultValue === 'Colors');
    if (colorsSection) {
      // Find the index of the color picker
      const colorPickerIndex = colorsSection.items.findIndex(item => item.messageKey === 'KEY_MINUTE_COLOR');
      // If found, remove it
      if (colorPickerIndex > -1) {
        colorsSection.items.splice(colorPickerIndex, 1);
      }
    }
  }
}

const clay = new Clay(clayConfig, manipulateConfig);