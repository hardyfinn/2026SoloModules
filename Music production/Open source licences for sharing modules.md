---
title: Open source licences for sharing modules
date: 2026-11-21
source: Music production · Sharing modules with the community
topic: Tech & Work
tags: [licensing, open-source, vcv-rack, mit]
summary: |
 Use MIT for modules that are wholly your own and GPL-3 only where you borrow GPL code.
 VCV allows any licence for free plugins; names and trademarks matter more than licence for the VCV Library.
---

# Open source licences for sharing modules

## Summary
Use MIT for modules that are wholly your own and GPL-3 only where you borrow GPL code.
VCV allows any licence for free plugins; names and trademarks matter more than licence for the VCV Library.

## Question
> New thread - sharing these models. We have made some really good modules with some really interesting ideas. I'd like to know your thoughts on how best to share these with the wider community. I'm interested to know the open source license that would help people build these in other formats, for example VST, Windows, etc.

## Answer
My recommendation is to put your own modules under the **MIT** licence, and keep **GPL-3** only for the ones built on someone else's GPL code. MIT is the licence that most helps people rebuild them as VST/CLAP plugins, Windows or Linux builds, or hardware like the disting NT. VCV allows MIT for any plugin that's free. The catch is that someone could sell a closed version without giving anything back. If that bothers you, staying on GPL still allows VST/CLAP ports, as long as they're open source too.

[…]

### Write-up (general sections)

#### Short answer

Use **MIT** for the modules that are wholly our own idea and code, and keep **GPL-3.0-or-later** for the ones that borrow GPL code. MIT is the licence that most helps people rebuild a module as a VST/CLAP plugin, a Windows or Linux build, or hardware firmware, because it lets them do anything as long as they keep your name and the licence text.

VCV allows this. Its rules say a plugin linking the Rack SDK must be GPL-3, **unless** it is free of charge, in which case the "Non-Commercial Plugin Licence Exception" lets you pick any licence, MIT included ([VCV Manual: Plugin Licensing](https://vcvrack.com/manual/PluginLicensing)).

The trade-off in one line: MIT means someone could sell a closed-source port without giving anything back; GPL means every port must stay open source. If that bothers you, stay on GPL everywhere (Option A below); it still allows VST/CLAP ports, just only open ones.

To share: tidy the repos (licence files, names, a `main` branch), add free GitHub builds for Mac, Windows and Linux, submit the strongest few to the VCV Library, and post on the VCV Community forum.

#### Licence options

| Option | What people may do | What it means for ports | Best for |
| --- | --- | --- | --- |
| **A. GPL-3.0-or-later everywhere** (what we have now) | Use, change, share, even sell, but anything they release must be GPL and open source too | VST/CLAP ports must be open source. Fine with DPF, iPlug2, JUCE (AGPL) and the VST3 and CLAP SDKs. Closed hardware can't use the code | Keeping every version of your ideas open forever |
| **B. MIT for our own modules, GPL where forced** (recommended) | Anything, including closed and commercial use, as long as your copyright line and licence text stay | Any format, any framework, any company. Most likely to get ported | Spreading the ideas as widely as possible |
| **C. Dual: Rack plugin GPL, DSP engine files MIT** | Engine code is free to reuse; the Rack panel code stays GPL | Porters take the engine and write their own interface, which they do anyway | A middle way if you want the VCV versions themselves to stay GPL |

Why B: a port to another format nearly always reuses only the sound engine (our `*Engine.hpp` files) and rebuilds the panel for the new host. MIT makes that engine free to drop into anything: a Steinberg VST3 plugin (the VST3 SDK itself became MIT in October 2025, [Steinberg](https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/VST3+License)), a CLAP plugin, a Daisy Patch or disting NT plugin, or a commercial Eurorack module.

One honest caveat: most of this code was written with an AI assistant. In some countries purely AI-written code may carry weaker copyright, which mainly matters for GPL (it relies on copyright to force openness). MIT is not hurt by that, which is another small point in its favour.

Whatever you pick, keep credit lines in every README; they cost nothing and the community values them.

#### Names and trademarks

The licence covers code, not names. VCV Library entry requires following its ethics rules, which say you may not clone the brand name, model name, logo, panel design or control layout of another product without the owner's permission, whether or not it is legally a trademark ([VCV Manual](https://vcvrack.com/manual/PluginLicensing)). Fine for a personal GitHub repo; a blocker for the Library.

Makers of small Eurorack modules often say yes when asked nicely, and a "made with permission" note is a good look in the community.

#### Where and how to share

| Route | What it gives | Effort |
| --- | --- | --- |
| **VCV Library** | One-click install inside Rack for every user, automatic updates. For free open-source plugins you send the source link and VCV builds Mac, Windows and Linux for you | Open an issue on VCV's library repo per plugin; follow the naming rules above; respond to their review notes |
| **GitHub Releases + automatic builds** | A download page per plugin with Mac (Intel and Apple Silicon), Windows and Linux files, rebuilt on every change | A shared build workflow added to each repo once; our builds today are Mac Apple Silicon only |
| **maxforlive.com** | The home for Max for Live devices (Razor, Loopshifter) | Upload the .amxd with the guide PDF |
| **VCV Community forum** | The main audience. A "new plugin" post with a panel picture, a 30 second audio or video clip and a demo patch gets real feedback and testers | One post per release |
| **lines (llllllll.co), ModWiggler, r/vcvrack** | Experimental and Eurorack crowds who'd enjoy Peal, Lilt, Ectoplasm, Gridworks | Cross-post the same material |

Two things that matter more than the licence for getting used:

- **Testing first.** Most modules are marked untested in Rack. Ship the ones you've played with, a few at a time, rather than 40 at once; early bugs in public stick to a name.
- **One brand.** Group them under "" as the Library brand, with consistent panels (you already have the logo and label standard). People follow a maker, not 40 separate plugins. Small single-module plugins like Odds, Lilt or Heathrow could also be merged into one or two collections.

#### How people would port them

A port keeps the sound engine and rebuilds the controls for the new host. That's easy here: in 13 of our plugins the engine already lives in its own `…Engine.hpp` file with no Rack code in it. Doing the same for the rest, and saying so in each README ("the DSP is in src/XEngine.hpp, MIT, no dependencies"), is the single biggest help to porters.

| Target | Usual tool | Licence fit |
| --- | --- | --- |
| VST3 / CLAP / AU / LV2 in any DAW, Windows, Mac, Linux | **Cardinal**: an open-source version of VCV Rack that runs as a plugin inside DAWs. Open-source plugins can be added to it with no rewrite | Needs GPL-3 compatible code; both GPL and MIT fit |
| A standalone VST3 / CLAP plugin | DPF (ISC), iPlug2 (zlib-style), JUCE (AGPL-3 or paid), nih-plug (ISC) | MIT engine: any of them, open or closed. GPL engine: open source only, JUCE under its AGPL option |
| disting NT | Expert Sleepers' C++ plugin API | MIT engine drops straight in. Your own rig, so a natural first port |
| Other hardware (Daisy Patch, Bela, a commercial module) | libDaisy (MIT), Bela | MIT allows a closed product; GPL forces the firmware to be published |
| Max for Live | gen\~ codebox, as we did for Razor | Either licence |

Cardinal is worth singling out: it already ships hundreds of open-source VCV plugins inside Ableton and other DAWs, and getting ours into it gives VST/CLAP versions for Windows, Mac and Linux without anyone writing a port.

#### Sources

- [VCV Manual: Plugin Licensing](https://vcvrack.com/manual/PluginLicensing) (GPL rule, free-plugin exception, ethics rules)
- [Steinberg: VST 3 Licence](https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/VST3+License) (VST3 SDK under MIT since 3.8, October 2025)

## Sources
- [VCV Manual: Plugin Licensing](https://vcvrack.com/manual/PluginLicensing)
- [Steinberg: VST 3 Licence](https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/VST3+License)
