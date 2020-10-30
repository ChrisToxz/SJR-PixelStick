// ==ClosureCompiler==
// @output_file_name default.js
// @compilation_level SIMPLE_OPTIMIZATIONS
// ==/ClosureCompiler==

// Closure compiler used to minimise

// PixelStick JavaScript
// (c) Steve Russell 2020
var btns = document.querySelectorAll(".topnav a:not(#select)");
var pages = document.querySelectorAll(".page");
var config;   // Holds the user config data as a JSON object after the websocket is opened
var fixPresets; // Holds the fixed colour presets
var linkedPicker = 0; // Picker 0 is linked to the RGB slider to start with
var ws;
var browserInit = true;

function startSocket() {
  // console.log("Starting websocket...");
  ws = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);


  ws.onopen = function () {
    // connection.send('Connect ' + new Date());
    // console.log('WebSocket opened');
    sendCmd('C'); // Request the config data
    sendCmd('F'); // Get the fixed colour presets
    document.getElementById("sktled").style.backgroundColor = "#80ff00";
    document.getElementsByTagName("body").disabled = false;
  };
  ws.onerror = function (error) {
    // console.log('WebSocket Error: ', error);
    alert("Websocket closed");
  };
  ws.onmessage = function (e) {
    switch (e.data.substr(0, 1)) {
      case 'V': // Voltage reading
        updateVoltage(e.data.substr(1));
        break;
      case 'U': // Requested update complete
        updateComplete(e.data.substr(1));
        break;
      case 'B': // Bitmap file lists
        fillBmpList(e.data.substr(1));
        break;
      case 'S': // System info
        fillSysinfo(e.data.substr(1));
        break;
      case 'C': // [C]onfig data to initialise at start up
        initPage(e.data.substr(1));
        break;
      case 'F': // [F]ixed preset data to initialise at start up
        initFixPresets(e.data.substr(1));
        break;
      case 'I': // [I]nit of browser complete
        document.getElementById("save").disabled = e.data[1] == '1' ? false : true;
        break;
      case "?": // Some kind of problem
        errorHandler(e.data);
        break;
      default: // Unknown response
        errorHandler("?Unknown response: " + e.data);
    }
  };
  ws.onclose = function () {
    // console.log('WebSocket connection closed');
    document.getElementById("sktled").style.backgroundColor = "#ff0000";
  };
}

// Made a separate function as it's easier to document the commands
// Valid commands are:
//    "B"           get a list of available BMP files
//    "C"           get the configuration data for initialisation
//    "I"           get the user changes status ([I]nit end)
//    "S"           get file system info
//    "U0"          switch the LEDs off
//    "U1"          switch the LEDs on
//    "UA<ssid:pw>" update AP mode credentials
//    "UB<colour><val>"     set Blue channel for colour 0-4 to value
//    "UC<ssid:pw>" update Client mode credentials
//    "UD<path>"    draw the current bitmap file
//    "UE<0|1>""     set repeat off/on
//    "UF<path>"    select a new bitmap file and get its info
//    "UG<colour><val>"     set Green channel for colour 0-4  to value
//    "UH<index>"   set the indexed fixed colour preset active
//    "UI<val>"     set brightness to value
//    "UJ<val>"     set number of colours used in fixed mode
//    "UK<0|1>"     set gradient off/on
//    "UL<val>"     set switch delay to value
//    "UM<mode>"    set mode (fixed/preset/bitmap) 
//    "UN<0|1>"     set interleave off/on
//    "UO<index><name>" store a new fixed colour preset
//    "UP<index>"   set the index for the presets
//    "UQ<index>"   set the index for the palettes of the current preset
//    "UR<colour><val>"     set Red channel for colour 0-4 to value
//    "US"          save the current settings in the file system
//    "UT<val>"     set the row refresh time
//    "UU<n><val>"  set the value for parameter n
//    "UX<path>"    delete the specified file
function sendCmd(request) {
  // console.log(request);
  ws.send(request);
}

function updateVoltage(data) {
  // Voltage formula based on ADC linearity measurements
  var voltage = 4 + (data - 480) * 0.00869;
  var image;
  var batticon = document.getElementById("batticon");

  document.getElementById("voltage").innerHTML = voltage.toFixed(1) + 'V';
  batticon.className = "";
  if (voltage > 7.7) image = "images/batt-100.png";
  else if (voltage > 7.3) image = "images/batt-075.png";
  else if (voltage > 6.9) image = "images/batt-050.png";
  else if (voltage > 6.6) image = "images/batt-025.png";
  else {
    image = "images/batt-010.png";
    batticon.className = "blinking";
  }
  batticon.src = image;
}

function updateComplete(data) {
  var updateSettings = true;
  switch (data[0]) {
    case '0':
    case '1':
      config.ledson = data[0] == '1' ? true : false;
      setPowerSwitch();
      updateSettings = false;
      break;
    case 'A': // AP credentials  
      var creds = data.substr(1).split(":");
      config.apssid = creds[0];
      config.appw = creds[1];
      break;
    case 'B': // Blue  
      config.colours[data[1]][2] = data.substr(2);
      break;
    case 'C':
      document.getElementById("store").disabled = true;
      alert("Client credentials updated");
      updateSettings = false;
      break;
    case 'D': // Draw bitmap doesn't update settings
      updateSettings = false;
      break;
    case 'E': // Repeat - doesn't update settings
      updateSettings = false;
      break;
    case 'F': // Data for the current bitmap file
      fillFileData(data.substr(1));
      if (browserInit) { // This is the last call during init
        browserInit = false;
        sendCmd('I'); // Get the user changes status (might have been an earlier connection)
        updateSettings = false; // In which case, don't change the save status
      }
      break;
    case 'G': // Green  
      config.colours[data[1]][1] = data.substr(2);
      break;
    case 'H': // Fixed colour preset  
      syncPreset(data[1]);
      break;
    case 'I': // Brightness  
      config.brightness = data.substr(1);
      break;
    case 'J': // Colours used  
      config.coloursused = data[1];
      break;
    case 'K': // Gradient  
      config.gradient = data[1] == '1' ? true : false;
      break;
    case 'L': // Switch delay  
      config.delay = data.substr(1);
      break;
    case 'M': // Change mode (mode is saved only if other settings also changed)
      updateSettings = false;
      break;
    case 'N': // Interleave  
      config.interleave = data[1] == '1' ? true : false;
      break;
    case 'O': // Save fixed colour preset
      syncActiveColours(data)
      break;
    case 'P': // Preset index  
      config.presetidx = data.substr(1);
      showParms();
      break;
    case 'Q': // Palette index  
      config.presets[config.presetidx].paletteidx = data.substr(1);
      showParms();
      break;
    case 'R': // Red  
      config.colours[data[1]][0] = data.substr(2);
      break;
    case 'S':
      updateSettings = false;
      document.getElementById("save").disabled = true;
      break;
    case 'T': // Row display time  
      config.rowtime = data.substr(1);
      break;
    case 'U': // Preset parameter  
      config.presets[config.presetidx].parms[data[1]].values[2] = data.substr(2);
      break;
    case 'X': // Delete a bitmap file
      var list = document.getElementById('delete');
      var idx = list.selectedIndex;
      list.remove(idx)
      document.getElementById('bitmaps').remove(idx);
      sendCmd('S'); // Update the file system info
      var newBmp = document.getElementById("bitmaps").value;
      if (newBmp != "")
        setCurrentBmp(newBmp);
      break;
    case '?': // Some kind of problem   
      errorHandler(data);
      updateSettings = false;
      break;
    default:
  }
  // User has changed something so give the option to save
  if (updateSettings)
    document.getElementById("save").disabled = false;
}

function setActivePage(navid) {
  if (document.getElementById(navid).classList.contains('active')) return; // Already active so do nothing
  if (navid == "fixed" || navid == "preset" || navid == "bitmap") {
    switch (navid) {
      case "fixed":
        config.mode = 0;
        break;
      case "preset":
        config.mode = 1;
        break;
      case "bitmap":
        config.mode = 2;
        break;
      default:
    }
    sendCmd("UM" + config.mode);
  }
  btns.forEach(btn => {
    if (btn.id == navid) {
      btn.classList.add('active');
    }
    else {
      btn.classList.remove('active');
    }
  })
  pages.forEach(page => {
    if (page.id == navid + "panel") {
      page.style.display = "";
    }
    else {
      page.style.display = "none";
    }
  })
  if (document.getElementById("myTopnav").className !== "topnav") {
    toggleMenu();
  }
}

function toggleMenu() {
  var x = document.getElementById("myTopnav");
  if (x.className === "topnav") {
    x.className += " responsive";
    document.getElementById("menuicon").src = "images/x.png";
  } else {
    x.className = "topnav";
    document.getElementById("menuicon").src = "images/menu.png";
  }
}

function toggleLEDs() {
  if (document.getElementById("power").className === "pushbtn") {
    sendCmd("U1");
  }
  else {
    sendCmd("U0");
  }
}

function setPowerSwitch() {
  var x = document.getElementById("power");
  var y = document.getElementById("bitmaps");
  var w = document.getElementById("delbtn");
  if (config.ledson == 1) {
    x.className += " on";
    y.disabled = true;
    w.disabled = true;
  } else {
    x.className = "pushbtn";
    y.disabled = false;
    w.disabled = false;
    looping = false;
    document.getElementById("draw").innerHTML = "Draw";
  }
}

function saveSettings() {
  sendCmd("US");
}

function setDrawTime() {
  document.getElementById("drawtime").innerHTML = (document.getElementById("timsld").value * bmpHeight / 1000).toFixed(1) + "s";
}

var setSliderTimer = {};

function setSlider(sldid) {
  clearTimeout(setSliderTimer);
  setSliderTimer = setTimeout(function () {
    var s = 'U';
    switch (sldid) {
      case "redsld":
        s += 'R';
        s += linkedPicker;
        break;
      case "grnsld":
        s += 'G';
        s += linkedPicker;
        break;
      case "blusld":
        s += 'B';
        s += linkedPicker;
        break;
      case "britesld":
        s += 'I';
        break;
      case "timsld":
        s += 'T';
        setDrawTime();
        break;
      case "p0sld":
        s += 'U0';
        break;
      case "p1sld":
        s += 'U1';
        break;
      case "p2sld":
        s += 'U2';
        break;
    }
    sendCmd(s + document.getElementById(sldid).value);
  }, 50);
}

// Update the colour value, colour sliders and sample when the picker changes
function updatePicker(picker, update) {
  var pickerNo = picker.id[6];
  document.getElementById("pickedcolour" + pickerNo).innerHTML = picker.value.toUpperCase();
  if (pickerNo == linkedPicker) {
    var redval = parseInt("0x" + picker.value.slice(1, 3));
    var grnval = parseInt("0x" + picker.value.slice(3, 5));
    var bluval = parseInt("0x" + picker.value.slice(5));
    updateSliderValue("redsld", redval);
    updateSliderValue("grnsld", grnval);
    updateSliderValue("blusld", bluval);
    if (update) {
      setSlider("redsld");
      setTimeout(() => setSlider("grnsld"), 55);
      setTimeout(() => setSlider("blusld"), 110);
    }
    sample.style.backgroundColor = picker.value;
  }
  else {
    config.colours[pickerNo][0] = parseInt("0x" + picker.value.slice(1, 3));
    config.colours[pickerNo][1] = parseInt("0x" + picker.value.slice(3, 5));
    config.colours[pickerNo][2] = parseInt("0x" + picker.value.slice(5));
    sendCmd("UR" + pickerNo + config.colours[pickerNo][0]);
    setTimeout(() => sendCmd("UG" + pickerNo + config.colours[pickerNo][1]), 55);
    setTimeout(() => sendCmd("UB" + pickerNo + config.colours[pickerNo][2]), 110);
  }
}

function setColoursUsed(num) {
  sendCmd("UJ" + num);
}

function setGradient(state) {
  sendCmd("UK" + (state ? "1" : "0"));
}

function setInterleave(state) {
  if (state) {
    document.getElementById("gradient").disabled = true;
  }
  else {
    document.getElementById("gradient").disabled = false;
  }
  sendCmd("UN" + (state ? "1" : "0"));
}

function setPicker(pickerNo) {
  var picker = document.getElementById("picker" + pickerNo);

  linkedPicker = pickerNo;
  updatePicker(picker, false);
  sample.style.backgroundColor = picker.value;
}

function updateColour(pickerNo) {
  var picker = document.getElementById("picker" + pickerNo);
  var redsld = document.getElementById("redsld");
  var grnsld = document.getElementById("grnsld");
  var blusld = document.getElementById("blusld");
  var x = (redsld.value << 16 | grnsld.value << 8 | blusld.value).toString(16);
  while (x.length < 6) {
    x = "0" + x;
  }
  picker.value = "#" + x;
  document.getElementById("pickedcolour" + linkedPicker).innerHTML = picker.value.toUpperCase();
  sample.style.backgroundColor = picker.value;
}

function updateSliderValue(elemid, elemval) {
  var elem = document.getElementById(elemid);
  elem.value = elemval;
  document.getElementById(elemid + "val").innerHTML = elemval;
  if (elemid == "redsld" || elemid == "grnsld" || elemid == "blusld") {
    updateColour(linkedPicker)
  }
}

function togglePw(elemid) {
  var x = document.getElementById(elemid);
  if (x.type === "password") {
    x.type = "text";
  } else {
    x.type = "password";
  }
}

function checkValid(elemid, minlen, maxlen) {
  if (minlen < 1) minlen = 1;
  var patt = new RegExp("^([a-z]|[A-Z]|[0-9]|[-_,'#£$% ]!){" + minlen + "," + maxlen + "}$");
  if (!patt.test(document.getElementById(elemid).value)) {
    alert("Invalid: Only alphanumeric and '-_,'#£$% ]!' characters allowed and must be between " + minlen + " and " + maxlen + " characters long");
    if (elemid.startsWith("client"))
      document.getElementById("store").disabled = true;
    if (elemid == "fixname")
      document.getElementById("fpsave").disabled = true;
  }
  else {
    if (elemid.startsWith("ap")) {
      var s = "UA"; // Update AP credentials
      s += document.getElementById("apssid").value;
      s += ':';
      s += document.getElementById("appw").value;
      sendCmd(s);
    }
    if (elemid.startsWith("client")) {
      if (document.getElementById("clientssid").value != "" && document.getElementById("clientpw").value != "")
        document.getElementById("store").disabled = false;
    }
    if (elemid == "fixname")
      document.getElementById("fpsave").disabled = false;
  }
}

function checkNumber(elemid, minval, maxval) {
  var newvalue = Number(document.getElementById(elemid).value);
  if (newvalue < minval || newvalue > maxval)
    alert("Invalid: Must be a value between " + minval + " and " + maxval);
  else {
    sendCmd("UL" + newvalue);
  }
}

function saveCredentials() {
  var s = "UC";
  s += document.getElementById("clientssid").value;
  s += ':';
  s += document.getElementById("clientpw").value;
  sendCmd(s);
}

// For uploads, check filename is OK
function checkFilename() {
  var fullPath = document.getElementById('upload').value;
  if (fullPath) {
    var startIndex = (fullPath.indexOf('\\') >= 0 ? fullPath.lastIndexOf('\\') : fullPath.lastIndexOf('/'));
    var filename = fullPath.substring(startIndex);
    if (filename.indexOf('\\') === 0 || filename.indexOf('/') === 0) {
      filename = filename.substring(1);
    }
  }
  var x = document.getElementById("subupload");
  if (filename.length < 32 && filename.endsWith(".bmp"))
    x.disabled = false;
  else {
    x.disabled = true;
    alert("Must be a *.bmp file with a name less than 32 characters long");
  }
}

function setFixPreset(index) {
  sendCmd("UH" + document.getElementById("fixpresets").selectedIndex)
}

function syncActiveColours(data) {
  var index = data[1];
  fixPresets[index].name = data.substr(2);
  fixPresets[index].coloursused = config.coloursused;
  fixPresets[index].gradient = config.gradient;
  fixPresets[index].interleave = config.interleave;
  for (var i = 0; i < config.coloursused; i++) {
    fixPresets[index].colours[i][0] = config.colours[i][0];
    fixPresets[index].colours[i][1] = config.colours[i][1];
    fixPresets[index].colours[i][2] = config.colours[i][2];
  }
  document.getElementById("fpsave").disabled = true;
  document.getElementById("fixname").value = "";
  var option = document.getElementById("fixpresets").options[data[1]];
  option.value = option.text = data.substr(2);
}

function syncPreset(index) {
  config.coloursused = fixPresets[index].coloursused;
  config.gradient = fixPresets[index].gradient;
  config.interleave = fixPresets[index].interleave;
  for (var i = 0; i < config.coloursused; i++) {
    config.colours[i][0] = fixPresets[index].colours[i][0];
    config.colours[i][1] = fixPresets[index].colours[i][1];
    config.colours[i][2] = fixPresets[index].colours[i][2];
    var picker = document.getElementById("picker" + i);
    var x = (config.colours[i][0] << 16 | config.colours[i][1] << 8 | config.colours[i][2]).toString(16);
    while (x.length < 6) {
      x = "0" + x;
    }
    picker.value = "#" + x;
    document.getElementById("pickedcolour" + i).innerHTML = picker.value.toUpperCase();
    if (i == linkedPicker) {
      // Update the sliders and the sample
      document.getElementById("redsld").value = config.colours[i][0];
      document.getElementById("redsldval").innerHTML = config.colours[i][0];
      document.getElementById("grnsld").value = config.colours[i][1];
      document.getElementById("grnsldval").innerHTML = config.colours[i][1];
      document.getElementById("blusld").value = config.colours[i][2];
      document.getElementById("blusldval").innerHTML = config.colours[i][2];
      sample.style.backgroundColor = picker.value;
    }
  }
  document.getElementById("col" + config.coloursused).checked = true;
  document.getElementById("gradient").checked = config.gradient;
  document.getElementById("gradient").disabled = config.interleave ? true : false;
  document.getElementById("interleave").checked = config.interleave;

}

function saveFixPreset() {
  console.log("Save preset");
  var s = "Enter the slot number you want to store this preset in:\n\n";
  var i = 0;
  for (x of fixPresets) {
    s += i + " - " + x.name + "\n";
    i++;
  }
  var response = prompt(s);
  if (response && response != "") { // Will fail if user cancels or doesn't enter anything
    response = parseInt(response);
    if (Number.isInteger(response) && response >= 0 && response <= 7) {
      s = "UO";
      s += response;
      s += document.getElementById("fixname").value;
      sendCmd(s);
    }
    else {
      alert("Please enter an integer between 0 and 7");
    }
  }
}

// Load the fixed colour presets from data held by the server
function initFixPresets(fpString) {
  // console.log(fpString);
  fixPresets = JSON.parse(fpString);
  var s = document.getElementById("fixpresets");
  for (x of fixPresets) {
    var opt = document.createElement("option");
    opt.text = x.name;
    opt.value = x.name;
    s.add(opt);
  }
  s.selectedIndex = -1;
}

// Load initial values for the user controls from the config data held by the server
function initPage(configString) {
  // console.log(configString);
  config = JSON.parse(configString);
  // Put the power switch in the correct state
  setPowerSwitch(config.ledson);
  // Initialise the Brightness slider value
  updateSliderValue("britesld", config.brightness);
  // Initialise the switch delay
  document.getElementById("delay").value = config.delay;
  // Set the colours used radio button
  document.getElementById("col" + config.coloursused).checked = true;
  // Set the gradient switch
  document.getElementById("gradient").checked = config.gradient;
  // Set the interleave switch
  document.getElementById("interleave").checked = config.interleave;
  document.getElementById("gradient").disabled = config.interleave; // Disable gradient if interleaved
  // Initialise the colour slider values (picker0 and sample are updated by them)
  updateSliderValue("redsld", config.colours[0][0]);
  updateSliderValue("grnsld", config.colours[0][1]);
  updateSliderValue("blusld", config.colours[0][2]);
  // Initialise the other pickers
  for (var i = 1; i < 5; i++) {
    var picker = document.getElementById('picker' + i);
    var x = (config.colours[i][0] << 16 | config.colours[i][1] << 8 | config.colours[i][2]).toString(16);
    while (x.length < 6) {
      x = "0" + x;
    }
    picker.value = "#" + x;
    document.getElementById("pickedcolour" + i).innerHTML = picker.value.toUpperCase();
  }
  // Load the presets and palettes from the config data
  fillPresets();
  // Initialise the bitmap row time slider
  updateSliderValue("timsld", config.rowtime);
  sendCmd("S"); // Get file system info
  sendCmd("B"); // Get list of bitmap files
  // Load the AP data
  document.getElementById("apssid").value = config.apssid;
  document.getElementById("appw").value = config.appw;
  // Set the active page
  var page;
  switch (config.mode) {
    case 0:
      page = "fixed";
      break;
    case 1:
      page = "preset";
      break;
    case 2:
      page = "bitmap";
      break;
    default:
      errorHandler("?Invalid mode: " + mode);
  }
  setActivePage(page);
}

// Get the list of BMP files from the server to populate the select lists 
// in the bitmaps and files panels. I discovered you can't share options 
// between selects, so we have to create separate options for each list.
function fillBmpList(data) {
  // console.log("Filelist:", data)
  var files = data.split(":");
  filesel = document.getElementById("bitmaps");
  delsel = document.getElementById("delete");
  for (x of files) {
    var fopt = document.createElement("option");
    var dopt = document.createElement("option");
    fopt.text = x;
    fopt.value = "/bmp/" + x; // BMP files are stored in the /bmp/ directory
    dopt.text = x;
    dopt.value = "/bmp/" + x;
    if (config.bmpfile.endsWith(x)) {
      fopt.selected = true;
      sendCmd("UF" + fopt.value);
    }
    filesel.add(fopt);
    delsel.add(dopt);
  }
  delsel.selectedIndex = -1;
}

function setPreset() {
  sendCmd("UP" + document.getElementById("presets").selectedIndex);
}

function setCurrentBmp(path) {
  if (config.bmpfile != path) {
    sendCmd("UF" + path);
    config.bmpfile = path;
  }
}

function deleteFile() {
  var fname = document.getElementById("delete").value;
  if (fname != "") {
    if (confirm("Delete " + fname + "?")) {
      sendCmd("UX" + fname);
    }
  }
}

var looping = false;

function drawBitmap() {
  var s = "UD0"; // Draw command + end loop
  config.ledson = true;
  setPowerSwitch();
  var btntext;
  if (document.getElementById("repeat").checked && !looping) {
    s = "UD1";
    looping = true;
    btntext = "Stop";
  } else {
    looping = false;
    btntext = "Draw";
  }
  sendCmd(s);
  document.getElementById("draw").innerHTML = btntext;
}

function setRepeat(state) {
  sendCmd("UE" + (state ? "1" : "0"));
}

var bmpWidth; // Save bitmap width and height for display time and preview calculation
var bmpHeight;

function fillFileData(data) {
  var fd = data.split(":");
  bmpWidth = fd[0];
  bmpHeight = fd[1];
  var s = fd[0] + "px x " + fd[1] + "px";  // Width, Height
  document.getElementById("currentfile").innerHTML = s;
  showPreview();
  setDrawTime();
}

function showPreview() {
  var scalefactor = (window.innerWidth - 20) / bmpHeight;
  var xform = scalefactor >= 1 ? "translate(0, -100%)" : "translate(0, -" + scalefactor * 100 + "%) scale(" + scalefactor + ")";

  document.getElementById("preview").src = document.getElementById("bitmaps").value;
  document.getElementById("preview").style.transformOrigin = "0 0";
  document.getElementById("preview").style.transform = "rotate(90deg) " + xform;
  if (scalefactor <= 1)
    document.getElementById("prevtxt").style.transform = "translate(0, " + (scalefactor - 1) * bmpWidth + "px)";
  else
    document.getElementById("prevtxt").style.transform = "translate(0, 0)";
}

function fillPresets() {
  // console.log("Presets:", data)
  var p = config.presets;
  var s = document.getElementById("presets");
  for (x of p) {
    var opt = document.createElement("option");
    opt.text = x.name;
    opt.value = x.name;
    s.add(opt);
  }
  s.selectedIndex = config.presetidx;

  p = config.palettes;
  s = document.getElementById("palette");
  for (x of p) {
    var opt = document.createElement("option");
    opt.text = x;
    opt.value = x;
    s.add(opt);
  }
  showParms()
}

function showParms() {
  var preset = config.presets[config.presetidx];
  // Switch off all parms to begin with
  document.getElementById("parms").style.display = "none";
  document.getElementById("parm0").style.display = "none";
  document.getElementById("parm1").style.display = "none";
  document.getElementById("parm2").style.display = "none";
  document.getElementById("palettes").style.display = "none";
  var parmNo = 0;

  if (typeof preset.parms !== "undefined") {
    for (parm of preset.parms) {
      document.getElementById("p" + parmNo + "name").innerHTML = parm.name;
      document.getElementById("p" + parmNo + "sld").min = parm.values[0];
      document.getElementById("p" + parmNo + "sld").max = parm.values[1];
      updateSliderValue("p" + parmNo + "sld", parm.values[2]);
      document.getElementById("parm" + parmNo).style.display = "";
      parmNo += 1;
    }
    document.getElementById("parms").style.display = "";
  }
  if (typeof preset.paletteidx !== "undefined") {
    document.getElementById("palettes").style.display = "";
    document.getElementById("palette").selectedIndex = preset.paletteidx;
    document.getElementById("parms").style.display = "";
  }
}

function setPalette() {
  sendCmd("UQ" + document.getElementById("palette").selectedIndex);
}

function fillSysinfo(data) {
  document.getElementById("fsinfo").innerHTML = data;
}

function errorHandler(data) {
  // console.log(data);
  alert(data.substr(1));
}