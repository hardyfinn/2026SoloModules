// Loads the device sources into one scope, like the built device file does.
const fs = require('fs'), vm = require('vm'), path = require('path');
module.exports = function (files) {
  const ctx = { console, Math, Array, Object };
  vm.createContext(ctx);
  for (const f of files) vm.runInContext(fs.readFileSync(path.join(__dirname, '../src', f), 'utf8'), ctx, { filename: f });
  return ctx;
};
