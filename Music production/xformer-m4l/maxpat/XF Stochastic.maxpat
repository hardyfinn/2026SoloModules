{
 "patcher": {
  "fileversion": 1,
  "appversion": {
   "major": 9,
   "minor": 0,
   "revision": 0,
   "architecture": "x64",
   "modernui": 1
  },
  "classnamespace": "box",
  "rect": [
   60.0,
   80.0,
   1100.0,
   1000.0
  ],
  "bglocked": 0,
  "openinpresentation": 1,
  "default_fontsize": 10.0,
  "default_fontface": 0,
  "default_fontname": "Arial Bold",
  "gridonopen": 1,
  "gridsize": [
   15.0,
   15.0
  ],
  "gridsnaponopen": 1,
  "objectsnaponopen": 1,
  "statusbarvisible": 2,
  "toolbarvisible": 1,
  "lefttoolbarpinned": 0,
  "toptoolbarpinned": 0,
  "righttoolbarpinned": 0,
  "bottomtoolbarpinned": 0,
  "toolbars_unpinned_last_save": 0,
  "tallnewobj": 0,
  "boxanimatetime": 200,
  "enablehscroll": 1,
  "enablevscroll": 1,
  "devicewidth": 554.0,
  "description": "",
  "digest": "",
  "tags": "",
  "style": "",
  "subpatcher_template": "",
  "assistshowspatchername": 0,
  "boxes": [
   {
    "box": {
     "maxclass": "newobj",
     "text": "v8 xf_stochastic.js",
     "numinlets": 1,
     "numoutlets": 5,
     "outlettype": [
      "",
      "",
      "",
      "",
      ""
     ],
     "patching_rect": [
      300,
      420,
      120,
      22.0
     ],
     "saved_object_attributes": {
      "parameter_enable": 0
     },
     "id": "obj-1"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "r ---xfp",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      300,
      380,
      68,
      22.0
     ],
     "id": "obj-2"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "s ---xfp",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      40,
      900,
      68,
      22.0
     ],
     "id": "obj-3"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "clock: 1 slot = 5 ticks = 1/96 note, only while Live plays",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      40,
      20,
      340,
      20.0
     ],
     "id": "obj-4"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "metro 5 ticks @active 1 @quantize 5 ticks",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      "bang"
     ],
     "patching_rect": [
      40,
      50,
      230,
      22.0
     ],
     "id": "obj-5"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "transport",
     "numinlets": 2,
     "numoutlets": 9,
     "outlettype": [
      "int",
      "int",
      "float",
      "float",
      "float",
      "",
      "int",
      "float",
      ""
     ],
     "patching_rect": [
      40,
      85,
      75,
      22.0
     ],
     "id": "obj-6"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "expr int($f1 / 5. + 0.5)",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      40,
      120,
      150,
      22.0
     ],
     "id": "obj-7"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "t i i i",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "int",
      "int",
      "int"
     ],
     "patching_rect": [
      40,
      155,
      61,
      22.0
     ],
     "id": "obj-8"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "coll",
     "numinlets": 1,
     "numoutlets": 4,
     "outlettype": [
      "",
      "",
      "",
      ""
     ],
     "patching_rect": [
      160,
      230,
      40,
      22.0
     ],
     "saved_object_attributes": {
      "embed": 0,
      "precision": 6
     },
     "id": "obj-9"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend remove",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      120,
      195,
      110,
      22.0
     ],
     "id": "obj-10"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend pos",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      40,
      195,
      89,
      22.0
     ],
     "id": "obj-11"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "zl iter 2",
     "numinlets": 2,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "patching_rect": [
      160,
      265,
      75,
      22.0
     ],
     "id": "obj-12"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "midiformat",
     "numinlets": 7,
     "numoutlets": 2,
     "outlettype": [
      "int",
      ""
     ],
     "patching_rect": [
      160,
      300,
      82,
      22.0
     ],
     "id": "obj-13"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "midiflush",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      "int"
     ],
     "patching_rect": [
      160,
      335,
      75,
      22.0
     ],
     "id": "obj-14"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "midiout",
     "numinlets": 1,
     "numoutlets": 0,
     "outlettype": [],
     "patching_rect": [
      160,
      370,
      61,
      22.0
     ],
     "id": "obj-15"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "change 0.",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "int",
      "int"
     ],
     "patching_rect": [
      300,
      120,
      75,
      22.0
     ],
     "id": "obj-16"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend tempo",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      300,
      155,
      103,
      22.0
     ],
     "id": "obj-17"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "zl change",
     "numinlets": 2,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "patching_rect": [
      420,
      120,
      75,
      22.0
     ],
     "id": "obj-18"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend timesig",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      420,
      155,
      117,
      22.0
     ],
     "id": "obj-19"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "s ---xfd",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      420,
      470,
      68,
      22.0
     ],
     "id": "obj-20"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "pattr xfstate",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      ""
     ],
     "patching_rect": [
      540,
      470,
      103,
      22.0
     ],
     "varname": "xfstate",
     "saved_object_attributes": {
      "parameter_enable": 1,
      "parameter_mappable": 0
     },
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "xfstate",
       "parameter_shortname": "xfstate",
       "parameter_type": 3,
       "parameter_invisible": 1
      }
     },
     "id": "obj-21"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend xfstate",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      540,
      505,
      117,
      22.0
     ],
     "id": "obj-22"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "live.thisdevice",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "bang",
      "int",
      "int"
     ],
     "patching_rect": [
      560,
      20,
      117,
      22.0
     ],
     "id": "obj-23"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "t b b b",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "bang",
      "bang",
      "bang"
     ],
     "patching_rect": [
      560,
      55,
      61,
      22.0
     ],
     "id": "obj-24"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "path live_set",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      640,
      90,
      105,
      22.0
     ],
     "id": "obj-25"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "live.path",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      ""
     ],
     "patching_rect": [
      640,
      125,
      75,
      22.0
     ],
     "id": "obj-26"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "property is_playing",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      560,
      125,
      147,
      22.0
     ],
     "id": "obj-27"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "live.observer",
     "numinlets": 2,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "patching_rect": [
      560,
      160,
      103,
      22.0
     ],
     "saved_object_attributes": {
      "_persistence": 1
     },
     "id": "obj-28"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend playing",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      560,
      195,
      117,
      22.0
     ],
     "id": "obj-29"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "loaded",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      700,
      90,
      56,
      22.0
     ],
     "id": "obj-30"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "midiin",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      "int"
     ],
     "patching_rect": [
      800,
      20,
      54,
      22.0
     ],
     "id": "obj-31"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "midiparse",
     "numinlets": 1,
     "numoutlets": 8,
     "outlettype": [
      "",
      "",
      "",
      "int",
      "int",
      "",
      "int",
      ""
     ],
     "patching_rect": [
      800,
      55,
      75,
      22.0
     ],
     "id": "obj-32"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend midinote",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      800,
      90,
      124,
      22.0
     ],
     "id": "obj-33"
    }
   },
   {
    "box": {
     "maxclass": "jsui",
     "filename": "xf_display.js",
     "jsarguments": [
      "stochastic",
      "main"
     ],
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "parameter_enable": 0,
     "patching_rect": [
      40,
      560,
      240,
      110
     ],
     "presentation": 1,
     "presentation_rect": [
      4,
      4,
      236,
      116
     ],
     "border": 0,
     "id": "obj-34"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "r ---xfd",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      40,
      530,
      68,
      22.0
     ],
     "id": "obj-35"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "route open",
     "numinlets": 2,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "patching_rect": [
      40,
      690,
      82,
      22.0
     ],
     "id": "obj-36"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "open",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      40,
      720,
      42,
      22.0
     ],
     "id": "obj-37"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "pcontrol",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      40,
      760,
      68,
      22.0
     ],
     "id": "obj-38"
    }
   },
   {
    "box": {
     "maxclass": "live.text",
     "varname": "btn_reset",
     "text": "Reset",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "patching_rect": [
      220,
      690,
      60,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      4.0,
      126,
      56.0,
      18
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Reset",
       "parameter_shortname": "Reset",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1,
       "parameter_invisible": 1
      }
     },
     "id": "obj-39"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "route bang 1",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      ""
     ],
     "patching_rect": [
      220,
      716,
      96,
      22.0
     ],
     "id": "obj-40"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "act reset",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      220,
      742,
      77,
      22.0
     ],
     "id": "obj-41"
    }
   },
   {
    "box": {
     "maxclass": "live.text",
     "varname": "btn_newRhythm",
     "text": "New Rhythm",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "patching_rect": [
      310,
      690,
      60,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      64.0,
      126,
      56.0,
      18
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "New Rhythm",
       "parameter_shortname": "New Rhythm",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1,
       "parameter_invisible": 1
      }
     },
     "id": "obj-42"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "route bang 1",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      ""
     ],
     "patching_rect": [
      310,
      716,
      96,
      22.0
     ],
     "id": "obj-43"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "act newRhythm",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      310,
      742,
      105,
      22.0
     ],
     "id": "obj-44"
    }
   },
   {
    "box": {
     "maxclass": "live.text",
     "varname": "btn_newMelody",
     "text": "New Melody",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "patching_rect": [
      400,
      690,
      60,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      124.0,
      126,
      56.0,
      18
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "New Melody",
       "parameter_shortname": "New Melody",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1,
       "parameter_invisible": 1
      }
     },
     "id": "obj-45"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "route bang 1",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      ""
     ],
     "patching_rect": [
      400,
      716,
      96,
      22.0
     ],
     "id": "obj-46"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "act newMelody",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      400,
      742,
      105,
      22.0
     ],
     "id": "obj-47"
    }
   },
   {
    "box": {
     "maxclass": "live.text",
     "varname": "btn_editor",
     "text": "Editor",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "patching_rect": [
      140,
      690,
      60,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      184.0,
      126,
      56.0,
      18
     ],
     "parameter_enable": 0,
     "id": "obj-48"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "route bang 1",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      ""
     ],
     "patching_rect": [
      140,
      715,
      96,
      22.0
     ],
     "id": "obj-49"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "click the display or Editor for the big window",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      0,
      0,
      10,
      20.0
     ],
     "presentation": 1,
     "presentation_rect": [
      4,
      148,
      236,
      16
     ],
     "textcolor": [
      0.75,
      0.75,
      0.75,
      1.0
     ],
     "fontsize": 9.0,
     "id": "obj-50"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "complexity",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "float"
     ],
     "patching_rect": [
      300,
      600,
      44,
      48
     ],
     "presentation": 1,
     "presentation_rect": [
      248,
      4,
      44,
      48
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Complexity",
       "parameter_shortname": "Complexity",
       "parameter_type": 1,
       "parameter_mmin": 0,
       "parameter_mmax": 100,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        50
       ],
       "parameter_unitstyle": 5
      }
     },
     "id": "obj-51"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p complexity",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      300,
      660,
      152,
      22.0
     ],
     "id": "obj-52"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "contour",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "float"
     ],
     "patching_rect": [
      360,
      600,
      44,
      48
     ],
     "presentation": 1,
     "presentation_rect": [
      298,
      4,
      44,
      48
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Contour",
       "parameter_shortname": "Contour",
       "parameter_type": 1,
       "parameter_mmin": -100,
       "parameter_mmax": 100,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ],
       "parameter_unitstyle": 0
      }
     },
     "id": "obj-53"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p contour",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      360,
      660,
      131,
      22.0
     ],
     "id": "obj-54"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "range",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "float"
     ],
     "patching_rect": [
      420,
      600,
      44,
      48
     ],
     "presentation": 1,
     "presentation_rect": [
      348,
      4,
      44,
      48
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Range",
       "parameter_shortname": "Range",
       "parameter_type": 1,
       "parameter_mmin": 0,
       "parameter_mmax": 100,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        50
       ],
       "parameter_unitstyle": 5
      }
     },
     "id": "obj-55"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p range",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      420,
      660,
      117,
      22.0
     ],
     "id": "obj-56"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "variation",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "float"
     ],
     "patching_rect": [
      480,
      600,
      44,
      48
     ],
     "presentation": 1,
     "presentation_rect": [
      398,
      4,
      44,
      48
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Variation",
       "parameter_shortname": "Variation",
       "parameter_type": 1,
       "parameter_mmin": 0,
       "parameter_mmax": 100,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        16
       ],
       "parameter_unitstyle": 5
      }
     },
     "id": "obj-57"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p variation",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      480,
      660,
      145,
      22.0
     ],
     "id": "obj-58"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "rest",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "float"
     ],
     "patching_rect": [
      540,
      720,
      44,
      48
     ],
     "presentation": 1,
     "presentation_rect": [
      248,
      62,
      44,
      48
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Rest",
       "parameter_shortname": "Rest",
       "parameter_type": 1,
       "parameter_mmin": 0,
       "parameter_mmax": 100,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ],
       "parameter_unitstyle": 5
      }
     },
     "id": "obj-59"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p rest",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      540,
      780,
      110,
      22.0
     ],
     "id": "obj-60"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "gateLength",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "float"
     ],
     "patching_rect": [
      600,
      720,
      44,
      48
     ],
     "presentation": 1,
     "presentation_rect": [
      298,
      62,
      44,
      48
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Gate",
       "parameter_shortname": "Gate",
       "parameter_type": 1,
       "parameter_mmin": 0,
       "parameter_mmax": 100,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ],
       "parameter_unitstyle": 5
      }
     },
     "id": "obj-61"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p gateLength",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      600,
      780,
      152,
      22.0
     ],
     "id": "obj-62"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "burst",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "float"
     ],
     "patching_rect": [
      660,
      720,
      44,
      48
     ],
     "presentation": 1,
     "presentation_rect": [
      348,
      62,
      44,
      48
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Burst",
       "parameter_shortname": "Burst",
       "parameter_type": 1,
       "parameter_mmin": 0,
       "parameter_mmax": 100,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ],
       "parameter_unitstyle": 5
      }
     },
     "id": "obj-63"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p burst",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      660,
      780,
      117,
      22.0
     ],
     "id": "obj-64"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "slide",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "float"
     ],
     "patching_rect": [
      720,
      720,
      44,
      48
     ],
     "presentation": 1,
     "presentation_rect": [
      398,
      62,
      44,
      48
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Slide",
       "parameter_shortname": "Slide",
       "parameter_type": 1,
       "parameter_mmin": 0,
       "parameter_mmax": 100,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ],
       "parameter_unitstyle": 5
      }
     },
     "id": "obj-65"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p slide",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      720,
      780,
      117,
      22.0
     ],
     "id": "obj-66"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Rhythm",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      800,
      600,
      80,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      2.0,
      96,
      12
     ],
     "textjustification": 0,
     "fontsize": 9.0,
     "id": "obj-67"
    }
   },
   {
    "box": {
     "maxclass": "live.tab",
     "varname": "rhythmMode",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      "float"
     ],
     "patching_rect": [
      800,
      630,
      100,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      14.0,
      96,
      15
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Rhythm",
       "parameter_shortname": "Rhythm",
       "parameter_type": 2,
       "parameter_enum": [
        "Loop",
        "Live"
       ],
       "parameter_mmax": 1,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        1
       ],
       "parameter_unitstyle": 9
      }
     },
     "num_lines_patching": 1,
     "num_lines_presentation": 1,
     "id": "obj-68"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p rhythmMode",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      800,
      656,
      152,
      22.0
     ],
     "id": "obj-69"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Melody",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      910,
      600,
      80,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      29.5,
      96,
      12
     ],
     "textjustification": 0,
     "fontsize": 9.0,
     "id": "obj-70"
    }
   },
   {
    "box": {
     "maxclass": "live.tab",
     "varname": "melodyMode",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      "float"
     ],
     "patching_rect": [
      910,
      630,
      100,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      41.5,
      96,
      15
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Melody",
       "parameter_shortname": "Melody",
       "parameter_type": 2,
       "parameter_enum": [
        "Loop",
        "Live"
       ],
       "parameter_mmax": 1,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        1
       ],
       "parameter_unitstyle": 9
      }
     },
     "num_lines_patching": 1,
     "num_lines_presentation": 1,
     "id": "obj-71"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p melodyMode",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      910,
      656,
      152,
      22.0
     ],
     "id": "obj-72"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Length",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      1020,
      600,
      80,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      57.0,
      96,
      12
     ],
     "textjustification": 0,
     "fontsize": 9.0,
     "id": "obj-73"
    }
   },
   {
    "box": {
     "maxclass": "live.menu",
     "varname": "noteDuration",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      "float"
     ],
     "patching_rect": [
      1020,
      630,
      100,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      69.0,
      96,
      15
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Length",
       "parameter_shortname": "Length",
       "parameter_type": 2,
       "parameter_enum": [
        "8x",
        "4x",
        "3x",
        "2x",
        "4/3x",
        "1x",
        "2/3x",
        "1/2x"
       ],
       "parameter_mmax": 7,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        5
       ]
      }
     },
     "id": "obj-74"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p noteDuration",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      1020,
      656,
      166,
      22.0
     ],
     "id": "obj-75"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Rate",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      1130,
      600,
      80,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      84.5,
      96,
      12
     ],
     "textjustification": 0,
     "fontsize": 9.0,
     "id": "obj-76"
    }
   },
   {
    "box": {
     "maxclass": "live.menu",
     "varname": "divisor",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      "float"
     ],
     "patching_rect": [
      1130,
      630,
      100,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      96.5,
      96,
      15
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Rate",
       "parameter_shortname": "Rate",
       "parameter_type": 2,
       "parameter_enum": [
        "1/64",
        "1/32",
        "1/16T",
        "1/16",
        "1/8T",
        "1/16.",
        "1/8",
        "1/4T",
        "1/8.",
        "1/4",
        "1/2",
        "1 bar",
        "2 bars",
        "4 bars"
       ],
       "parameter_mmax": 13,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        3
       ]
      }
     },
     "id": "obj-77"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p divisor",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      1130,
      656,
      131,
      22.0
     ],
     "id": "obj-78"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Scale",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      1240,
      600,
      80,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      112.0,
      96,
      12
     ],
     "textjustification": 0,
     "fontsize": 9.0,
     "id": "obj-79"
    }
   },
   {
    "box": {
     "maxclass": "live.menu",
     "varname": "scale",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      "float"
     ],
     "patching_rect": [
      1240,
      630,
      100,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      124.0,
      96,
      15
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Scale",
       "parameter_shortname": "Scale",
       "parameter_type": 2,
       "parameter_enum": [
        "Chromatic",
        "Major",
        "Harm. Minor",
        "Major Blues",
        "Minor Blues",
        "Major Pent.",
        "Minor Pent.",
        "Folk",
        "Japanese",
        "Gamelan",
        "Gypsy",
        "Arabian",
        "Flamenco",
        "Whole Tone"
       ],
       "parameter_mmax": 13,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        1
       ]
      }
     },
     "id": "obj-80"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p scale",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      1240,
      656,
      117,
      22.0
     ],
     "id": "obj-81"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Root",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      1350,
      600,
      80,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      139.5,
      96,
      12
     ],
     "textjustification": 0,
     "fontsize": 9.0,
     "id": "obj-82"
    }
   },
   {
    "box": {
     "maxclass": "live.menu",
     "varname": "rootNote",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      "float"
     ],
     "patching_rect": [
      1350,
      630,
      100,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      151.5,
      96,
      15
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Root",
       "parameter_shortname": "Root",
       "parameter_type": 2,
       "parameter_enum": [
        "C",
        "C#",
        "D",
        "D#",
        "E",
        "F",
        "F#",
        "G",
        "G#",
        "A",
        "A#",
        "B"
       ],
       "parameter_mmax": 11,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "id": "obj-83"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p rootNote",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      1350,
      656,
      138,
      22.0
     ],
     "id": "obj-84"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "p editor",
     "numinlets": 1,
     "numoutlets": 0,
     "outlettype": [],
     "patching_rect": [
      40,
      800,
      68,
      22.0
     ],
     "id": "obj-85",
     "patcher": {
      "fileversion": 1,
      "appversion": {
       "major": 9,
       "minor": 0,
       "revision": 0,
       "architecture": "x64",
       "modernui": 1
      },
      "classnamespace": "box",
      "rect": [
       100.0,
       100.0,
       840.0,
       542.0
      ],
      "bglocked": 0,
      "openinpresentation": 1,
      "default_fontsize": 10.0,
      "default_fontface": 0,
      "default_fontname": "Arial Bold",
      "gridonopen": 1,
      "gridsize": [
       15.0,
       15.0
      ],
      "gridsnaponopen": 1,
      "objectsnaponopen": 1,
      "statusbarvisible": 0,
      "toolbarvisible": 0,
      "lefttoolbarpinned": 0,
      "toptoolbarpinned": 0,
      "righttoolbarpinned": 0,
      "bottomtoolbarpinned": 0,
      "toolbars_unpinned_last_save": 0,
      "tallnewobj": 0,
      "boxanimatetime": 200,
      "enablehscroll": 0,
      "enablevscroll": 0,
      "devicewidth": 0.0,
      "description": "",
      "digest": "",
      "tags": "",
      "style": "",
      "subpatcher_template": "",
      "assistshowspatchername": 0,
      "boxes": [
       {
        "box": {
         "maxclass": "panel",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          0,
          0,
          60,
          30
         ],
         "presentation": 1,
         "presentation_rect": [
          0,
          0,
          840,
          542
         ],
         "bgcolor": [
          0.14,
          0.14,
          0.14,
          1.0
         ],
         "background": 1,
         "mode": 0,
         "border": 0,
         "id": "obj-1"
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "inlet",
         "numinlets": 0,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          10,
          10,
          47,
          22.0
         ],
         "id": "obj-2"
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "s ---xfp",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          600,
          700,
          68,
          22.0
         ],
         "id": "obj-3"
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "r ---xfd",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          600,
          40,
          68,
          22.0
         ],
         "id": "obj-4"
        }
       },
       {
        "box": {
         "maxclass": "jsui",
         "filename": "xf_display.js",
         "jsarguments": [
          "stochastic",
          "editor"
         ],
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "parameter_enable": 0,
         "patching_rect": [
          600,
          80,
          400,
          200
         ],
         "presentation": 1,
         "presentation_rect": [
          10,
          10,
          820,
          300
         ],
         "border": 0,
         "id": "obj-5"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "repeatProb",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          20,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          10,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Repeat",
           "parameter_shortname": "Repeat",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-6",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p repeatProb",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          20,
          460,
          152,
          22.0
         ],
         "id": "obj-7"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "legatoProb",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          90,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          68,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Legato",
           "parameter_shortname": "Legato",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-8",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p legatoProb",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          90,
          460,
          152,
          22.0
         ],
         "id": "obj-9"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "burstRate",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          160,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          126,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Burst Rate",
           "parameter_shortname": "Burst Rate",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            50
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-10",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p burstRate",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          160,
          460,
          145,
          22.0
         ],
         "id": "obj-11"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "burstCount",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          230,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          184,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Burst Cnt",
           "parameter_shortname": "Burst Cnt",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-12",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p burstCount",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          230,
          460,
          152,
          22.0
         ],
         "id": "obj-13"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "sleep",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          300,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          242,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Sleep",
           "parameter_shortname": "Sleep",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-14",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p sleep",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          300,
          460,
          117,
          22.0
         ],
         "id": "obj-15"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "mutate",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          370,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          300,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Mutate",
           "parameter_shortname": "Mutate",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-16",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p mutate",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          370,
          460,
          124,
          22.0
         ],
         "id": "obj-17"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "jump",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          440,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          358,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Jump",
           "parameter_shortname": "Jump",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-18",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p jump",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          440,
          460,
          110,
          22.0
         ],
         "id": "obj-19"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "patienceRhythm",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          510,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          416,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Patience R",
           "parameter_shortname": "Patience R",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            100
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-20",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p patienceRhythm",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          510,
          460,
          180,
          22.0
         ],
         "id": "obj-21"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "patienceMelody",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          580,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          474,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Patience M",
           "parameter_shortname": "Patience M",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            100
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-22",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p patienceMelody",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          580,
          460,
          180,
          22.0
         ],
         "id": "obj-23"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "marblesSpread",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          650,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          532,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Spread",
           "parameter_shortname": "Spread",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            50
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-24",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p marblesSpread",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          650,
          460,
          173,
          22.0
         ],
         "id": "obj-25"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "marblesBias",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          720,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          590,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Bias",
           "parameter_shortname": "Bias",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            50
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-26",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p marblesBias",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          720,
          460,
          159,
          22.0
         ],
         "id": "obj-27"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "maskRhythm",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          790,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          648,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Mask R",
           "parameter_shortname": "Mask R",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            100
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-28",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p maskRhythm",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          790,
          460,
          152,
          22.0
         ],
         "id": "obj-29"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "tiltRhythm",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          860,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          706,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Tilt R",
           "parameter_shortname": "Tilt R",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            50
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-30",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p tiltRhythm",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          860,
          460,
          152,
          22.0
         ],
         "id": "obj-31"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "maskMelody",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          930,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          764,
          320,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Mask M",
           "parameter_shortname": "Mask M",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            100
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-32",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p maskMelody",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          930,
          460,
          152,
          22.0
         ],
         "id": "obj-33"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "tiltMelody",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1000,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          10,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Tilt M",
           "parameter_shortname": "Tilt M",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-34",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p tiltMelody",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1000,
          560,
          152,
          22.0
         ],
         "id": "obj-35"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "feel",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1070,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          68,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Feel",
           "parameter_shortname": "Feel",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 100,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            50
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-36",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p feel",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1070,
          560,
          110,
          22.0
         ],
         "id": "obj-37"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "degreeRotation",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1140,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          126,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Deg Rot",
           "parameter_shortname": "Deg Rot",
           "parameter_type": 1,
           "parameter_mmin": -32,
           "parameter_mmax": 32,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 0
          }
         },
         "id": "obj-38",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p degreeRotation",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1140,
          560,
          180,
          22.0
         ],
         "id": "obj-39"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "maskRotation",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1210,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          184,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Mask Rot",
           "parameter_shortname": "Mask Rot",
           "parameter_type": 1,
           "parameter_mmin": -32,
           "parameter_mmax": 32,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 0
          }
         },
         "id": "obj-40",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p maskRotation",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1210,
          560,
          166,
          22.0
         ],
         "id": "obj-41"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "rotate",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1280,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          242,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Rotate",
           "parameter_shortname": "Rotate",
           "parameter_type": 1,
           "parameter_mmin": -64,
           "parameter_mmax": 64,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 0
          }
         },
         "id": "obj-42",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p rotate",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1280,
          560,
          124,
          22.0
         ],
         "id": "obj-43"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "size",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1350,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          300,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Steps",
           "parameter_shortname": "Steps",
           "parameter_type": 1,
           "parameter_mmin": 2,
           "parameter_mmax": 64,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            32
           ],
           "parameter_unitstyle": 0
          }
         },
         "id": "obj-44",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p size",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1350,
          560,
          110,
          22.0
         ],
         "id": "obj-45"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "first",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1420,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          358,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "First",
           "parameter_shortname": "First",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 63,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 0
          }
         },
         "id": "obj-46",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p first",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1420,
          560,
          117,
          22.0
         ],
         "id": "obj-47"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "octave",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1490,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          416,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Octave",
           "parameter_shortname": "Octave",
           "parameter_type": 1,
           "parameter_mmin": -10,
           "parameter_mmax": 10,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 0
          }
         },
         "id": "obj-48",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p octave",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1490,
          560,
          124,
          22.0
         ],
         "id": "obj-49"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "transpose",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1560,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          474,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Transp",
           "parameter_shortname": "Transp",
           "parameter_type": 1,
           "parameter_mmin": -48,
           "parameter_mmax": 48,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 0
          }
         },
         "id": "obj-50",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p transpose",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1560,
          560,
          145,
          22.0
         ],
         "id": "obj-51"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "scaleRotate",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1630,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          532,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Mode",
           "parameter_shortname": "Mode",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 11,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 0
          }
         },
         "id": "obj-52",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p scaleRotate",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1630,
          560,
          159,
          22.0
         ],
         "id": "obj-53"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "clockMult",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1700,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          590,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Clock Mult",
           "parameter_shortname": "Clock Mult",
           "parameter_type": 1,
           "parameter_mmin": 50,
           "parameter_mmax": 150,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            100
           ],
           "parameter_unitstyle": 5
          }
         },
         "id": "obj-54",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p clockMult",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1700,
          560,
          145,
          22.0
         ],
         "id": "obj-55"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "fixedVel",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1770,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          648,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Velocity",
           "parameter_shortname": "Velocity",
           "parameter_type": 1,
           "parameter_mmin": 1,
           "parameter_mmax": 127,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            100
           ],
           "parameter_unitstyle": 0
          }
         },
         "id": "obj-56",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p fixedVel",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1770,
          560,
          138,
          22.0
         ],
         "id": "obj-57"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "resetBars",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          1840,
          500,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          706,
          378,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Reset Bars",
           "parameter_shortname": "Reset Bars",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 32,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 0
          }
         },
         "id": "obj-58",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p resetBars",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          1840,
          560,
          145,
          22.0
         ],
         "id": "obj-59"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Burst Mode",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          20,
          650,
          80,
          18
         ],
         "presentation": 1,
         "presentation_rect": [
          10,
          440,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
         "id": "obj-60",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "live.tab",
         "varname": "burstHold",
         "numinlets": 1,
         "numoutlets": 3,
         "outlettype": [
          "",
          "",
          "float"
         ],
         "patching_rect": [
          20,
          600,
          100,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          10,
          454,
          150,
          16
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Burst Mode",
           "parameter_shortname": "Burst Mode",
           "parameter_type": 2,
           "parameter_enum": [
            "Hold fit",
            "Hold over",
            "Roll fit",
            "Roll over"
           ],
           "parameter_mmax": 3,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            1
           ],
           "parameter_unitstyle": 9
          }
         },
         "num_lines_patching": 1,
         "num_lines_presentation": 1,
         "id": "obj-61",
         "textcolor": [
          0.0,
          0.0,
          0.0,
          1.0
         ],
         "activetextcolor": [
          0.0,
          0.0,
          0.0,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p burstHold",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          20,
          626,
          145,
          22.0
         ],
         "id": "obj-62"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Evolve",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          130,
          650,
          80,
          18
         ],
         "presentation": 1,
         "presentation_rect": [
          174,
          440,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
         "id": "obj-63",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "live.tab",
         "varname": "evolve",
         "numinlets": 1,
         "numoutlets": 3,
         "outlettype": [
          "",
          "",
          "float"
         ],
         "patching_rect": [
          130,
          600,
          100,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          174,
          454,
          150,
          16
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Evolve",
           "parameter_shortname": "Evolve",
           "parameter_type": 2,
           "parameter_enum": [
            "Off",
            "On"
           ],
           "parameter_mmax": 1,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 9
          }
         },
         "num_lines_patching": 1,
         "num_lines_presentation": 1,
         "id": "obj-64",
         "textcolor": [
          0.0,
          0.0,
          0.0,
          1.0
         ],
         "activetextcolor": [
          0.0,
          0.0,
          0.0,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p evolve",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          130,
          626,
          124,
          22.0
         ],
         "id": "obj-65"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Slides",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          240,
          650,
          80,
          18
         ],
         "presentation": 1,
         "presentation_rect": [
          338,
          440,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
         "id": "obj-66",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "live.tab",
         "varname": "legato",
         "numinlets": 1,
         "numoutlets": 3,
         "outlettype": [
          "",
          "",
          "float"
         ],
         "patching_rect": [
          240,
          600,
          100,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          338,
          454,
          150,
          16
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Slides",
           "parameter_shortname": "Slides",
           "parameter_type": 2,
           "parameter_enum": [
            "Gap",
            "Legato"
           ],
           "parameter_mmax": 1,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            1
           ],
           "parameter_unitstyle": 9
          }
         },
         "num_lines_patching": 1,
         "num_lines_presentation": 1,
         "id": "obj-67",
         "textcolor": [
          0.0,
          0.0,
          0.0,
          1.0
         ],
         "activetextcolor": [
          0.0,
          0.0,
          0.0,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p legato",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          240,
          626,
          124,
          22.0
         ],
         "id": "obj-68"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Play From",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          350,
          650,
          80,
          18
         ],
         "presentation": 1,
         "presentation_rect": [
          502,
          440,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
         "id": "obj-69",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "live.tab",
         "varname": "resetOnStart",
         "numinlets": 1,
         "numoutlets": 3,
         "outlettype": [
          "",
          "",
          "float"
         ],
         "patching_rect": [
          350,
          600,
          100,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          502,
          454,
          150,
          16
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Play From",
           "parameter_shortname": "Play From",
           "parameter_type": 2,
           "parameter_enum": [
            "Here",
            "Song"
           ],
           "parameter_mmax": 1,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            1
           ],
           "parameter_unitstyle": 9
          }
         },
         "num_lines_patching": 1,
         "num_lines_presentation": 1,
         "id": "obj-70",
         "textcolor": [
          0.0,
          0.0,
          0.0,
          1.0
         ],
         "activetextcolor": [
          0.0,
          0.0,
          0.0,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p resetOnStart",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          350,
          626,
          166,
          22.0
         ],
         "id": "obj-71"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Reset by Note",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          460,
          650,
          80,
          18
         ],
         "presentation": 1,
         "presentation_rect": [
          666,
          440,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
         "id": "obj-72",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "live.tab",
         "varname": "rstAny",
         "numinlets": 1,
         "numoutlets": 3,
         "outlettype": [
          "",
          "",
          "float"
         ],
         "patching_rect": [
          460,
          600,
          100,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          666,
          454,
          150,
          16
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Reset by Note",
           "parameter_shortname": "Reset by Note",
           "parameter_type": 2,
           "parameter_enum": [
            "Off",
            "Any",
            "Pitch"
           ],
           "parameter_mmax": 2,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 9
          }
         },
         "num_lines_patching": 1,
         "num_lines_presentation": 1,
         "id": "obj-73",
         "textcolor": [
          0.0,
          0.0,
          0.0,
          1.0
         ],
         "activetextcolor": [
          0.0,
          0.0,
          0.0,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p rstAny",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          460,
          626,
          124,
          22.0
         ],
         "id": "obj-74"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Pitch",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          570,
          650,
          80,
          18
         ],
         "presentation": 1,
         "presentation_rect": [
          10,
          474,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
         "id": "obj-75",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "live.numbox",
         "varname": "rstPitch",
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          "float"
         ],
         "patching_rect": [
          570,
          600,
          44,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          10,
          488,
          50,
          16
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Pitch",
           "parameter_shortname": "Pitch",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 127,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            36
           ],
           "parameter_unitstyle": 8
          }
         },
         "id": "obj-76",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "prepend p rstPitch",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          570,
          626,
          138,
          22.0
         ],
         "id": "obj-77"
        }
       },
       {
        "box": {
         "maxclass": "live.text",
         "varname": "btn_undoRhythm",
         "text": "Undo Rhythm",
         "mode": 0,
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          ""
         ],
         "patching_rect": [
          20,
          760,
          60,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          10,
          512,
          90,
          18
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Undo Rhythm",
           "parameter_shortname": "Undo Rhythm",
           "parameter_type": 2,
           "parameter_enum": [
            "off",
            "on"
           ],
           "parameter_mmax": 1,
           "parameter_invisible": 1
          }
         },
         "id": "obj-78",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "route bang 1",
         "numinlets": 1,
         "numoutlets": 3,
         "outlettype": [
          "",
          "",
          ""
         ],
         "patching_rect": [
          20,
          786,
          96,
          22.0
         ],
         "id": "obj-79"
        }
       },
       {
        "box": {
         "maxclass": "message",
         "text": "act undoRhythm",
         "numinlets": 2,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          20,
          812,
          112,
          22.0
         ],
         "id": "obj-80"
        }
       },
       {
        "box": {
         "maxclass": "live.text",
         "varname": "btn_undoMelody",
         "text": "Undo Melody",
         "mode": 0,
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          ""
         ],
         "patching_rect": [
          110,
          760,
          60,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          108,
          512,
          90,
          18
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Undo Melody",
           "parameter_shortname": "Undo Melody",
           "parameter_type": 2,
           "parameter_enum": [
            "off",
            "on"
           ],
           "parameter_mmax": 1,
           "parameter_invisible": 1
          }
         },
         "id": "obj-81",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "route bang 1",
         "numinlets": 1,
         "numoutlets": 3,
         "outlettype": [
          "",
          "",
          ""
         ],
         "patching_rect": [
          110,
          786,
          96,
          22.0
         ],
         "id": "obj-82"
        }
       },
       {
        "box": {
         "maxclass": "message",
         "text": "act undoMelody",
         "numinlets": 2,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          110,
          812,
          112,
          22.0
         ],
         "id": "obj-83"
        }
       },
       {
        "box": {
         "maxclass": "live.text",
         "varname": "btn_newBoth",
         "text": "New Both",
         "mode": 0,
         "numinlets": 1,
         "numoutlets": 2,
         "outlettype": [
          "",
          ""
         ],
         "patching_rect": [
          200,
          760,
          60,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          206,
          512,
          90,
          18
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "New Both",
           "parameter_shortname": "New Both",
           "parameter_type": 2,
           "parameter_enum": [
            "off",
            "on"
           ],
           "parameter_mmax": 1,
           "parameter_invisible": 1
          }
         },
         "id": "obj-84",
         "textcolor": [
          0.95,
          0.95,
          0.95,
          1.0
         ]
        }
       },
       {
        "box": {
         "maxclass": "newobj",
         "text": "route bang 1",
         "numinlets": 1,
         "numoutlets": 3,
         "outlettype": [
          "",
          "",
          ""
         ],
         "patching_rect": [
          200,
          786,
          96,
          22.0
         ],
         "id": "obj-85"
        }
       },
       {
        "box": {
         "maxclass": "message",
         "text": "act newBoth",
         "numinlets": 2,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          200,
          812,
          91,
          22.0
         ],
         "id": "obj-86"
        }
       },
       {
        "box": {
         "maxclass": "comment",
         "text": "X|FORMER Stochastic editor",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          10,
          40,
          300,
          20.0
         ],
         "id": "obj-87"
        }
       }
      ],
      "lines": [
       {
        "patchline": {
         "source": [
          "obj-4",
          0
         ],
         "destination": [
          "obj-5",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-5",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-6",
          0
         ],
         "destination": [
          "obj-7",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-7",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-8",
          0
         ],
         "destination": [
          "obj-9",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-9",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-10",
          0
         ],
         "destination": [
          "obj-11",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-11",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-12",
          0
         ],
         "destination": [
          "obj-13",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-13",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-14",
          0
         ],
         "destination": [
          "obj-15",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-15",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-16",
          0
         ],
         "destination": [
          "obj-17",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-17",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-18",
          0
         ],
         "destination": [
          "obj-19",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-19",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-20",
          0
         ],
         "destination": [
          "obj-21",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-21",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-22",
          0
         ],
         "destination": [
          "obj-23",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-23",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-24",
          0
         ],
         "destination": [
          "obj-25",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-25",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-26",
          0
         ],
         "destination": [
          "obj-27",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-27",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-28",
          0
         ],
         "destination": [
          "obj-29",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-29",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-30",
          0
         ],
         "destination": [
          "obj-31",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-31",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-32",
          0
         ],
         "destination": [
          "obj-33",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-33",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-34",
          0
         ],
         "destination": [
          "obj-35",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-35",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-36",
          0
         ],
         "destination": [
          "obj-37",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-37",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-38",
          0
         ],
         "destination": [
          "obj-39",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-39",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-40",
          0
         ],
         "destination": [
          "obj-41",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-41",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-42",
          0
         ],
         "destination": [
          "obj-43",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-43",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-44",
          0
         ],
         "destination": [
          "obj-45",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-45",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-46",
          0
         ],
         "destination": [
          "obj-47",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-47",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-48",
          0
         ],
         "destination": [
          "obj-49",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-49",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-50",
          0
         ],
         "destination": [
          "obj-51",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-51",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-52",
          0
         ],
         "destination": [
          "obj-53",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-53",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-54",
          0
         ],
         "destination": [
          "obj-55",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-55",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-56",
          0
         ],
         "destination": [
          "obj-57",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-57",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-58",
          0
         ],
         "destination": [
          "obj-59",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-59",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-61",
          0
         ],
         "destination": [
          "obj-62",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-62",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-64",
          0
         ],
         "destination": [
          "obj-65",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-65",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-67",
          0
         ],
         "destination": [
          "obj-68",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-68",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-70",
          0
         ],
         "destination": [
          "obj-71",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-71",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-73",
          0
         ],
         "destination": [
          "obj-74",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-74",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-76",
          0
         ],
         "destination": [
          "obj-77",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-77",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-78",
          0
         ],
         "destination": [
          "obj-79",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-79",
          0
         ],
         "destination": [
          "obj-80",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-79",
          1
         ],
         "destination": [
          "obj-80",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-80",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-81",
          0
         ],
         "destination": [
          "obj-82",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-82",
          0
         ],
         "destination": [
          "obj-83",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-82",
          1
         ],
         "destination": [
          "obj-83",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-83",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-84",
          0
         ],
         "destination": [
          "obj-85",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-85",
          0
         ],
         "destination": [
          "obj-86",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-85",
          1
         ],
         "destination": [
          "obj-86",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-86",
          0
         ],
         "destination": [
          "obj-3",
          0
         ]
        }
       }
      ],
      "title": "XF Stochastic editor"
     },
     "saved_object_attributes": {
      "description": "",
      "digest": "",
      "globalpatchername": "",
      "tags": ""
     }
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "X|FORMER Stochastic for Live. Engines: xf_stochastic.js (v8), display: xf_display.js (jsui).",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      40,
      960,
      500,
      20.0
     ],
     "id": "obj-86"
    }
   }
  ],
  "lines": [
   {
    "patchline": {
     "source": [
      "obj-2",
      0
     ],
     "destination": [
      "obj-1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-5",
      0
     ],
     "destination": [
      "obj-6",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-6",
      7
     ],
     "destination": [
      "obj-7",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-7",
      0
     ],
     "destination": [
      "obj-8",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-8",
      2
     ],
     "destination": [
      "obj-9",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-8",
      1
     ],
     "destination": [
      "obj-10",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-10",
      0
     ],
     "destination": [
      "obj-9",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-8",
      0
     ],
     "destination": [
      "obj-11",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-11",
      0
     ],
     "destination": [
      "obj-1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-9",
      0
     ],
     "destination": [
      "obj-12",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-12",
      0
     ],
     "destination": [
      "obj-13",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-13",
      0
     ],
     "destination": [
      "obj-14",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-14",
      0
     ],
     "destination": [
      "obj-15",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-6",
      4
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-16",
      0
     ],
     "destination": [
      "obj-17",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-17",
      0
     ],
     "destination": [
      "obj-1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-6",
      5
     ],
     "destination": [
      "obj-18",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-18",
      0
     ],
     "destination": [
      "obj-19",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-19",
      0
     ],
     "destination": [
      "obj-1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-1",
      0
     ],
     "destination": [
      "obj-9",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-1",
      1
     ],
     "destination": [
      "obj-13",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-1",
      2
     ],
     "destination": [
      "obj-14",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-1",
      3
     ],
     "destination": [
      "obj-20",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-1",
      4
     ],
     "destination": [
      "obj-21",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-21",
      0
     ],
     "destination": [
      "obj-22",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-22",
      0
     ],
     "destination": [
      "obj-1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-23",
      0
     ],
     "destination": [
      "obj-24",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-24",
      2
     ],
     "destination": [
      "obj-25",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-25",
      0
     ],
     "destination": [
      "obj-26",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-24",
      1
     ],
     "destination": [
      "obj-27",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-27",
      0
     ],
     "destination": [
      "obj-28",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-26",
      0
     ],
     "destination": [
      "obj-28",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-28",
      0
     ],
     "destination": [
      "obj-29",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-29",
      0
     ],
     "destination": [
      "obj-1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-24",
      0
     ],
     "destination": [
      "obj-30",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-30",
      0
     ],
     "destination": [
      "obj-1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-31",
      0
     ],
     "destination": [
      "obj-32",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-32",
      0
     ],
     "destination": [
      "obj-33",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-33",
      0
     ],
     "destination": [
      "obj-1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-35",
      0
     ],
     "destination": [
      "obj-34",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-34",
      0
     ],
     "destination": [
      "obj-36",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-36",
      0
     ],
     "destination": [
      "obj-37",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-36",
      1
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-37",
      0
     ],
     "destination": [
      "obj-38",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-39",
      0
     ],
     "destination": [
      "obj-40",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-40",
      0
     ],
     "destination": [
      "obj-41",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-40",
      1
     ],
     "destination": [
      "obj-41",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-41",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-42",
      0
     ],
     "destination": [
      "obj-43",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-43",
      0
     ],
     "destination": [
      "obj-44",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-43",
      1
     ],
     "destination": [
      "obj-44",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-44",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-45",
      0
     ],
     "destination": [
      "obj-46",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-46",
      0
     ],
     "destination": [
      "obj-47",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-46",
      1
     ],
     "destination": [
      "obj-47",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-47",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-48",
      0
     ],
     "destination": [
      "obj-49",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-49",
      0
     ],
     "destination": [
      "obj-37",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-49",
      1
     ],
     "destination": [
      "obj-37",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-51",
      0
     ],
     "destination": [
      "obj-52",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-52",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-53",
      0
     ],
     "destination": [
      "obj-54",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-54",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-55",
      0
     ],
     "destination": [
      "obj-56",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-56",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-57",
      0
     ],
     "destination": [
      "obj-58",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-58",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-59",
      0
     ],
     "destination": [
      "obj-60",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-60",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-61",
      0
     ],
     "destination": [
      "obj-62",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-62",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-63",
      0
     ],
     "destination": [
      "obj-64",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-64",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-65",
      0
     ],
     "destination": [
      "obj-66",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-66",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-68",
      0
     ],
     "destination": [
      "obj-69",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-69",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-71",
      0
     ],
     "destination": [
      "obj-72",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-72",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-74",
      0
     ],
     "destination": [
      "obj-75",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-75",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-77",
      0
     ],
     "destination": [
      "obj-78",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-78",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-80",
      0
     ],
     "destination": [
      "obj-81",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-81",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-83",
      0
     ],
     "destination": [
      "obj-84",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-84",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-38",
      0
     ],
     "destination": [
      "obj-85",
      0
     ]
    }
   }
  ],
  "dependency_cache": [
   {
    "name": "xf_stochastic.js",
    "bootpath": ".",
    "patcherrelativepath": ".",
    "type": "TEXT",
    "implicit": 1
   },
   {
    "name": "xf_display.js",
    "bootpath": ".",
    "patcherrelativepath": ".",
    "type": "TEXT",
    "implicit": 1
   }
  ],
  "latency": 0,
  "is_mpe": 0,
  "minimum_live_version": "",
  "minimum_max_version": "",
  "platform_compatibility": 0,
  "autosave": 0
 }
}