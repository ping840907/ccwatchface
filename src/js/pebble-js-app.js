const Clay = require('pebble-clay');
const clayConfig = require('./config.json');

// 在打開配置頁面前操作 config 的函數
function manipulateConfig() {
  const watchInfo = Pebble.getActiveWatchInfo();
  // 檢查是否為黑白 (B&W) 平台
  const isBW = watchInfo && (watchInfo.platform === 'aplite' || watchInfo.platform === 'diorite');

  // 找到 "Colors" 區塊
  const colorsSection = clayConfig.find(item => item.type === 'section' && item.items[0].defaultValue === 'Colors');
  if (!colorsSection) return;

  // 找到顏色選擇器 (Accent Color) 的索引
  const colorPickerIndex = colorsSection.items.findIndex(item => item.messageKey === 'KEY_MINUTE_COLOR');

  if (isBW) {
    // 在 B&W 平台上，用 "Disable Accent" 開關替換顏色選擇器
    if (colorPickerIndex > -1) {
      colorsSection.items.splice(colorPickerIndex, 1, {
        "type": "toggle",
        "messageKey": "KEY_BW_ACCENT_OFF",
        "label": "Disable Accent", // 停用強調色
        "defaultValue": false,
        "description": "On B&W watches, this turns off the accent color, making the text solid." // 在黑白手錶上，這會關閉強調色，使文字變為純色。
      });
    }
  }
  // 在彩色平台上，顏色選擇器會保留 (根據 config.json 的預設值)，
  // 並且 "Disable Accent" 開關不會被添加。
}

const clay = new Clay(clayConfig, manipulateConfig);