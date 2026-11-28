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
     "text": "v8 xf_algo.js",
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
      "algo",
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
      76.0,
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
     "varname": "btn_reseed",
     "text": "Reseed",
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
      84.0,
      126,
      76.0,
      18
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Reseed",
       "parameter_shortname": "Reseed",
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
     "text": "act reseed",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      310,
      742,
      84,
      22.0
     ],
     "id": "obj-44"
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
      164.0,
      126,
      76.0,
      18
     ],
     "parameter_enable": 0,
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
      140,
      715,
      96,
      22.0
     ],
     "id": "obj-46"
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
     "id": "obj-47"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "flow",
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
       "parameter_longname": "Flow",
       "parameter_shortname": "Flow",
       "parameter_type": 1,
       "parameter_mmin": 0,
       "parameter_mmax": 16,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        4
       ],
       "parameter_unitstyle": 0
      }
     },
     "id": "obj-48"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p flow",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      300,
      660,
      110,
      22.0
     ],
     "id": "obj-49"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "ornament",
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
       "parameter_longname": "Ornament",
       "parameter_shortname": "Ornament",
       "parameter_type": 1,
       "parameter_mmin": 0,
       "parameter_mmax": 16,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        4
       ],
       "parameter_unitstyle": 0
      }
     },
     "id": "obj-50"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p ornament",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      360,
      660,
      138,
      22.0
     ],
     "id": "obj-51"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "power",
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
       "parameter_longname": "Power",
       "parameter_shortname": "Power",
       "parameter_type": 1,
       "parameter_mmin": 0,
       "parameter_mmax": 16,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        12
       ],
       "parameter_unitstyle": 0
      }
     },
     "id": "obj-52"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p power",
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
     "id": "obj-53"
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
       "parameter_longname": "Gate",
       "parameter_shortname": "Gate",
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
     "id": "obj-54"
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
      480,
      660,
      152,
      22.0
     ],
     "id": "obj-55"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "glide",
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
       "parameter_longname": "Glide",
       "parameter_shortname": "Glide",
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
     "id": "obj-56"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p glide",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      540,
      780,
      117,
      22.0
     ],
     "id": "obj-57"
    }
   },
   {
    "box": {
     "maxclass": "live.dial",
     "varname": "trill",
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
       "parameter_longname": "Trill",
       "parameter_shortname": "Trill",
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
     "id": "obj-58"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p trill",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      600,
      780,
      117,
      22.0
     ],
     "id": "obj-59"
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
     "id": "obj-60"
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
      660,
      780,
      124,
      22.0
     ],
     "id": "obj-61"
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
       "parameter_longname": "Transp",
       "parameter_shortname": "Transp",
       "parameter_type": 1,
       "parameter_mmin": -11,
       "parameter_mmax": 11,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ],
       "parameter_unitstyle": 7
      }
     },
     "id": "obj-62"
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
      720,
      780,
      145,
      22.0
     ],
     "id": "obj-63"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Algorithm",
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
      2,
      96,
      12
     ],
     "textjustification": 0,
     "fontsize": 9.0,
     "id": "obj-64"
    }
   },
   {
    "box": {
     "maxclass": "live.menu",
     "varname": "algorithm",
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
      14,
      96,
      15
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Algorithm",
       "parameter_shortname": "Algorithm",
       "parameter_type": 2,
       "parameter_enum": [
        "Test",
        "TriTrance",
        "Stomper",
        "Markov",
        "Chip1",
        "Chip2",
        "Wobble",
        "ScaleWalk",
        "Window",
        "Minimal",
        "Ganz",
        "Blake",
        "Aphex",
        "Autechre",
        "StepWave",
        "Turing"
       ],
       "parameter_mmax": 15,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        1
       ]
      }
     },
     "id": "obj-65"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p algorithm",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      800,
      656,
      145,
      22.0
     ],
     "id": "obj-66"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Loop",
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
      30,
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
     "maxclass": "live.menu",
     "varname": "loopLength",
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
      42,
      96,
      15
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Loop",
       "parameter_shortname": "Loop",
       "parameter_type": 2,
       "parameter_enum": [
        "Inf",
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
        "10",
        "11",
        "12",
        "13",
        "14",
        "15",
        "16",
        "19",
        "21",
        "24",
        "32",
        "35",
        "42",
        "48",
        "56",
        "64",
        "95",
        "96",
        "127",
        "128"
       ],
       "parameter_mmax": 29,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "id": "obj-68"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "prepend p loopLength",
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
     "id": "obj-69"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Rate",
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
      58,
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
      1020,
      630,
      100,
      15
     ],
     "presentation": 1,
     "presentation_rect": [
      454,
      70,
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
     "id": "obj-71"
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
      1020,
      656,
      131,
      22.0
     ],
     "id": "obj-72"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Scale",
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
      86,
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
     "varname": "scale",
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
      98,
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
     "id": "obj-74"
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
      1130,
      656,
      117,
      22.0
     ],
     "id": "obj-75"
    }
   },
   {
    "box": {
     "maxclass": "live.comment",
     "text": "Root",
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
      114,
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
     "varname": "rootNote",
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
      126,
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
     "id": "obj-77"
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
      1240,
      656,
      138,
      22.0
     ],
     "id": "obj-78"
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
     "id": "obj-79",
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
       464.0
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
          464
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
          "algo",
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
          280
         ],
         "border": 0,
         "id": "obj-5"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "start",
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
          300,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Start",
           "parameter_shortname": "Start",
           "parameter_type": 1,
           "parameter_mmin": 0,
           "parameter_mmax": 16,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 0
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
         "text": "prepend p start",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          20,
          460,
          117,
          22.0
         ],
         "id": "obj-7"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "stepTrill",
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
          300,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Step Trill",
           "parameter_shortname": "Step Trill",
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
         "text": "prepend p stepTrill",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          90,
          460,
          145,
          22.0
         ],
         "id": "obj-9"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "skew",
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
          300,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Skew",
           "parameter_shortname": "Skew",
           "parameter_type": 1,
           "parameter_mmin": -8,
           "parameter_mmax": 8,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 0
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
         "text": "prepend p skew",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          160,
          460,
          110,
          22.0
         ],
         "id": "obj-11"
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
          230,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          184,
          300,
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
         "text": "prepend p rotate",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          230,
          460,
          124,
          22.0
         ],
         "id": "obj-13"
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
          300,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          242,
          300,
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
         "text": "prepend p scaleRotate",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          300,
          460,
          159,
          22.0
         ],
         "id": "obj-15"
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
          370,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          300,
          300,
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
         "text": "prepend p clockMult",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          370,
          460,
          145,
          22.0
         ],
         "id": "obj-17"
        }
       },
       {
        "box": {
         "maxclass": "live.dial",
         "varname": "gateOffset",
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
          300,
          44,
          48
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Gate Offs",
           "parameter_shortname": "Gate Offs",
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
         "text": "prepend p gateOffset",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          440,
          460,
          152,
          22.0
         ],
         "id": "obj-19"
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
          510,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          416,
          300,
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
         "text": "prepend p fixedVel",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          510,
          460,
          138,
          22.0
         ],
         "id": "obj-21"
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
          580,
          400,
          44,
          48
         ],
         "presentation": 1,
         "presentation_rect": [
          474,
          300,
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
         "text": "prepend p resetBars",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          580,
          460,
          145,
          22.0
         ],
         "id": "obj-23"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Mask",
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
          362,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
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
         "maxclass": "live.menu",
         "varname": "maskParameter",
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
          376,
          150,
          16
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Mask",
           "parameter_shortname": "Mask",
           "parameter_type": 2,
           "parameter_enum": [
            "All",
            "2",
            "3",
            "5",
            "11",
            "19",
            "31",
            "43",
            "61",
            "89",
            "131",
            "197",
            "277",
            "409",
            "599",
            "None"
           ],
           "parameter_mmax": 15,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ]
          }
         },
         "id": "obj-25",
         "textcolor": [
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
         "text": "prepend p maskParameter",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          20,
          626,
          173,
          22.0
         ],
         "id": "obj-26"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Mask Time",
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
          362,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
         "id": "obj-27",
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
         "varname": "timeMode",
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
          376,
          150,
          16
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Mask Time",
           "parameter_shortname": "Mask Time",
           "parameter_type": 2,
           "parameter_enum": [
            "Free",
            "Qrt",
            "1.5Q",
            "3Qrt"
           ],
           "parameter_mmax": 3,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 9
          }
         },
         "num_lines_patching": 1,
         "num_lines_presentation": 1,
         "id": "obj-28",
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
         "text": "prepend p timeMode",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          130,
          626,
          138,
          22.0
         ],
         "id": "obj-29"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Mask Step",
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
          362,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
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
         "maxclass": "live.tab",
         "varname": "maskProgression",
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
          376,
          150,
          16
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Mask Step",
           "parameter_shortname": "Mask Step",
           "parameter_type": 2,
           "parameter_enum": [
            "None",
            "+1",
            "+5",
            "+7"
           ],
           "parameter_mmax": 3,
           "parameter_initial_enable": 1,
           "parameter_initial": [
            0
           ],
           "parameter_unitstyle": 9
          }
         },
         "num_lines_patching": 1,
         "num_lines_presentation": 1,
         "id": "obj-31",
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
         "text": "prepend p maskProgression",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          240,
          626,
          187,
          22.0
         ],
         "id": "obj-32"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Vel Source",
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
          362,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
         "id": "obj-33",
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
         "varname": "velMode",
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
          376,
          150,
          16
         ],
         "parameter_enable": 1,
         "saved_attribute_attributes": {
          "valueof": {
           "parameter_longname": "Vel Source",
           "parameter_shortname": "Vel Source",
           "parameter_type": 2,
           "parameter_enum": [
            "Algo",
            "Fixed"
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
         "id": "obj-34",
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
         "text": "prepend p velMode",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          350,
          626,
          131,
          22.0
         ],
         "id": "obj-35"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Slides",
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
          362,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
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
          460,
          600,
          100,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          666,
          376,
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
         "id": "obj-37",
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
          460,
          626,
          124,
          22.0
         ],
         "id": "obj-38"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Play From",
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
          396,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
         "id": "obj-39",
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
          570,
          600,
          100,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          10,
          410,
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
         "id": "obj-40",
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
          570,
          626,
          166,
          22.0
         ],
         "id": "obj-41"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Reset by Note",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          680,
          650,
          80,
          18
         ],
         "presentation": 1,
         "presentation_rect": [
          174,
          396,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
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
          680,
          600,
          100,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          174,
          410,
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
         "id": "obj-43",
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
          680,
          626,
          124,
          22.0
         ],
         "id": "obj-44"
        }
       },
       {
        "box": {
         "maxclass": "live.comment",
         "text": "Pitch",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          790,
          650,
          80,
          18
         ],
         "presentation": 1,
         "presentation_rect": [
          338,
          396,
          150,
          14
         ],
         "textjustification": 0,
         "fontsize": 9.0,
         "id": "obj-45",
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
          790,
          600,
          44,
          15
         ],
         "presentation": 1,
         "presentation_rect": [
          338,
          410,
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
         "text": "prepend p rstPitch",
         "numinlets": 1,
         "numoutlets": 1,
         "outlettype": [
          ""
         ],
         "patching_rect": [
          790,
          626,
          138,
          22.0
         ],
         "id": "obj-47"
        }
       },
       {
        "box": {
         "maxclass": "comment",
         "text": "X|FORMER Algo editor",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          10,
          40,
          300,
          20.0
         ],
         "id": "obj-48"
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
          "obj-26",
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
          "obj-38",
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
       }
      ],
      "title": "XF Algo editor"
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
     "text": "X|FORMER Algo for Live. Engines: xf_algo.js (v8), display: xf_display.js (jsui).",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      40,
      960,
      500,
      20.0
     ],
     "id": "obj-80"
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
      "obj-37",
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
      "obj-37",
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
      "obj-60",
      0
     ],
     "destination": [
      "obj-61",
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
      "obj-3",
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
      "obj-63",
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
      "obj-38",
      0
     ],
     "destination": [
      "obj-79",
      0
     ]
    }
   }
  ],
  "dependency_cache": [
   {
    "name": "xf_algo.js",
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