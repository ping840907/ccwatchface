const Clay = require('pebble-clay');
const clayConfig = require('./config.json');

// Function to manipulate the config before opening
function manipulateConfig() {
  const watchInfo = Pebble.getActiveWatchInfo();
  const isBW = watchInfo && (watchInfo.platform === 'aplite' || watchInfo.platform === 'diorite');

  // Find the "Colors" section
  const colorsSection = clayConfig.find(item => item.type === 'section' && item.items[0].defaultValue === 'Colors');
  if (!colorsSection) return;

  // Find the index of the color picker
  const colorPickerIndex = colorsSection.items.findIndex(item => item.messageKey === 'KEY_MINUTE_COLOR');

  if (isBW) {
    // On B&W platforms, remove the color picker
    if (colorPickerIndex > -1) {
      colorsSection.items.splice(colorPickerIndex, 1);
    }

    // And add the B&W accent toggle
    colorsSection.items.push({
      "type": "toggle",
      "messageKey": "KEY_BW_ACCENT_OFF",
      "label": "Disable Accent",
      "defaultValue": false,
      "description": "On B&W watches, this turns off the accent color, making the text solid."
    });
  }
  // On color platforms, the color picker is shown by default and the B&W toggle is not added.
}

const clay = new Clay(clayConfig, manipulateConfig);