---
title: Claude models for building VCV Rack plugins
date: 2026-11-19
source: Misc - non work non music · project chat (19–20 and 26 Nov 2026)
topic: AI & Claude
tags: [claude, model-choice, vcv-rack, dsp, token-usage]
summary: |
  Sonnet 5.5 is the best default for VCV Rack plugin code; use Opus 5.5 or Fable for medium-to-hard DSP, Haiku for repetitive panel/param chores.
  Having a bigger model write a short concrete spec for a smaller one to build saves tokens on larger builds; keep Fable to the final design spec only.
---

# Claude models for building VCV Rack plugins

## Summary
Sonnet 5.5 is the best default for VCV Rack plugin code; use Opus 5.5 or Fable for medium-to-hard DSP and Haiku for repetitive chores.
A bigger model writing a short concrete spec for a smaller one to build saves tokens on larger builds.

## Question
> New thread. Which model should I use when building vcv rack plugins?

## Answer
Sonnet 5.5 is the best default for VCV Rack plugins. It handles the C++ module code, the Rack SDK boilerplate, the plugin.json and the Makefile well, at a good speed and cost. Move up to Opus 5.5 (or Fable 5.2) for harder DSP work such as filters, anti-aliasing, oscillator sync or tricky real-time bugs. Haiku 5.5 is enough for repetitive jobs like panel SVG tweaks or adding params and ports. Whichever you pick, check anything audio-related by ear, since models can write plausible DSP that sounds wrong.

## Question 2
> Is there tokens efficiency in using opus to make instructions for sonnet to follow to build vcv modules?

## Answer 2
Yes, it can be, with a catch. Opus plans once and Sonnet does the many cheap build steps, so you pay the pricier rate only for a short spec. It works best when the plan is concrete: module list, params, ports, DSP approach, file layout and a few acceptance checks. Keep it short, since Sonnet rereads it every turn. It's not worth it for small modules, where Sonnet alone is quicker and cheaper. It also doesn't save much if Sonnet keeps hitting DSP problems the plan didn't foresee, because you'll go back to Opus anyway. A good middle route is to have Opus design only the tricky DSP parts and let Sonnet handle the rest.

## Question 3
> Do they higher models do a better job optioneering new areas for vcv plugins?

## Answer 3
Somewhat, yes. Higher models tend to give more original and better-reasoned ideas. They connect modular synthesis concepts, user workflow gaps and what's feasible in real-time DSP more convincingly. Smaller models tend to list familiar ideas, like another filter or sequencer. Two caveats apply. None of them can reliably tell you what already exists in the VCV library, so check that yourself. And you'll get more from the prompt than the model: give it your tastes, the modules you own, and the gaps that annoy you, and ask for 10 ideas with a rough difficulty each. For ideation I'd use Opus or Fable for one session, then hand the chosen idea to Sonnet to build.

## Question 4 (20 Nov 2026)
> Dis we talk about model efficency when building vcv rack modules?

## Answer 4
Yes, we covered it yesterday. Sonnet 5.5 is the best default for VCV Rack plugins, with Opus 5.5 or Fable 5.2 for harder DSP work and Haiku 5.5 for repetitive jobs like panel SVG tweaks. We also talked about having Opus write a short, concrete spec (modules, params, ports, DSP approach, acceptance checks) for Sonnet to build from. That saves tokens on larger builds, but not on small modules or if the plan misses DSP problems. Higher models also give better ideas when you're exploring new module concepts, but check the VCV library yourself for what already exists.

## Question 5 (20 Nov 2026)
> I haven't used sonnet for modules, so I'm probably wasting Tokens on Opus. Am I right that medium level DSP is probably better with Opus? What sort of usage multiple does Opus use creating these VCV plugins?

## Answer 5
Probably yes on both counts. Medium-complexity DSP, like a decent filter, an envelope with some nonlinearity or a simple reverb, is where Opus tends to pay off over Sonnet. Sonnet is more likely to produce DSP that compiles but sounds slightly off. For the surrounding work (UI, params, ports, boilerplate, panel tweaks), Sonnet does the job fine and you're likely overspending on Opus there.

On usage, from your project's model notes: Opus 5.5 costs about 2× Sonnet 5.5 ($4/$20 vs $2/$10 per million tokens in/out), so Sonnet is roughly half. Haiku is about 1/40th. In the app you pay a flat plan, but the bigger models use up your allowance roughly in proportion to those prices. Those are list prices, not measured VCV usage, and real use also depends on how many turns and how much code it rereads.

A sensible approach is to try Sonnet for one module and compare it by ear. If it gets the DSP wrong twice, switch just that part to Opus.

## Question 6 (26 Nov 2026)
> Which models should I use to best handle the tasks I commonly do? These revolve around creating VCV rack models for synthesis and sequencing. I've been using Fable to do the original thinking and discussion around what the module can do before letting Fable build a design. That design is then passed over to Opus for building. I'm running through my Fable tokens too fast. Any advice?

## Answer 6
Your Fable-then-Opus split is sound, but you can move more of the work down a tier.

**Suggested split for VCV Rack modules:**
- **Concept and DSP discussion:** do the brainstorming on Sonnet or Opus. Reserve Fable for the one step where it earns its cost: the final design spec for anything genuinely tricky, such as novel DSP, tricky sequencer logic or polyphony and timing edge cases.
- **Build (C++ plugin code, panel SVG, plugin.json):** Opus is a good fit. Sonnet is likely enough for simple modules or variations on one you've already built.
- **Small stuff (bug fixes, renaming, panel tweaks, boilerplate, docs):** Sonnet or Haiku.
- **Debugging a stubborn audio glitch:** Opus first, Fable only if it stalls.

**Ways to spend fewer Fable tokens:**
1. Do the "what could it do" chat elsewhere, then bring Fable a tight brief: goals, I/O, controls, constraints. Ask it for a compact spec, not a long discussion.
2. Ask Fable for the design in a structured, concise format (ports, params, algorithm steps, edge cases) so Opus can build straight from it.
3. Keep the Fable thread short. Once the spec is done, start a fresh thread for building rather than continuing it.
4. Reuse a template or earlier module as a reference so Fable doesn't re-derive the boilerplate.
5. Lower the effort setting for the design step unless the module is really complex.

I can set the project's default model for new threads to Opus or Sonnet so Fable only gets used when you pick it. Want me to do that?

## Question 7 (26 Nov 2026)
> I hadn't thought about starting a new thread for the build. Will the new thread be able to see the spec okay?

## Answer 7
Not automatically: a new thread starts fresh and can't see the design thread's conversation. It's easy to handle, though.

**Simplest way:** have the design thread save the spec as a file in the project's shared folder (for example a vcv-modules/ folder with one .md file per module). Then tell me to start the build thread, and I'll point it at that file. The spec is then complete, and the build thread doesn't carry the whole design discussion, which is the token saving.

**Alternative:** I can start the build thread myself and pass the spec in the instructions, but a saved file is better because you can edit it, reuse it, and come back to it later.

Want me to ask the design thread to save specs that way from now on?

## Question 8 (26 Nov 2026)
> Is there a way to add the VCV boilerplate work so that Fable never does this as a redesign?

## Answer 8
Yes. Two pieces would do it:

1. **Project instruction:** a standing rule that Fable threads only produce the module spec (ports, params, DSP logic, edge cases) and never plan or write VCV boilerplate such as plugin.json, Makefile, plugin.cpp/hpp, the model registration, panel SVG layout, or the standard widget and param config code. That would apply to every new thread.
2. **A boilerplate template:** a reference module in the project files (vcv-modules/_template) that Opus build threads copy from. Fable never has to design that part, and Opus builds faster too.

I can add the instruction now. For the template, I'd start a build thread (on Opus) to create it from the current Rack SDK conventions. Shall I do both?

## Sources
- No links were given in the answer.

Related: [[Choosing between Claude models]], [[Orchestrator models and module families]], [[Minimising token usage]]
