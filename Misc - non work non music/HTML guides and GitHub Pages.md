---
title: HTML guides and GitHub Pages
date: 2026-11-26
source: Misc - non work non music · project chat
topic: AI & Claude
tags: [html, user-guides, artifacts, github-pages, publishing]
summary: |
  HTML beats PDF for module guides: cheaper to generate, easier to edit, opens anywhere. Claude can publish private artifacts with an index page but can't make them public or host them.
  GitHub Pages turns a public repo into a free static site; because some modules are clones with copyrighted names, guides stay as private artifacts; Sonnet is enough for PDF-to-HTML conversion.
---

# HTML guides and GitHub Pages

## Summary
HTML is cheaper, easier to edit and easier to share than PDF for module guides; Claude publishes them as private artifacts with one index page.
GitHub Pages would make a free public site, but guides stay private because some modules are clones.

## Question
> When I make modules, I tend to have Opus create a PDF as a guide. This is okay. However, I wonder if HTML would be cheaper for tokens, more easily edited and more easily shared. Can you let me know if Claude could upload these to a website so all my user guides are online?

## Answer
**HTML vs PDF:** HTML is the better choice on all three counts. It's cheaper because a PDF usually needs a layout and rendering script plus fix-up rounds, while HTML is just the text and a little CSS. It's easier to edit, since small changes are a one-line edit rather than regenerating the file. And it's easier to share, because it opens in any browser, works on phones, and can link between guides.

**Putting them online:** Claude can publish each guide as a page (an artifact) with its own link. These start private, and you choose whether to share them. What I can't do is change who can see a page, make it public, or put it on your own domain. You'd need to do the sharing yourself. A single index page linking all your module guides is easy to add. If you want a real public website, the usual route is a free host such as GitHub Pages or Netlify, which Claude can prepare files for, but you'd handle the upload and account.

If you'd like, I can add a rule so Opus builds guides as HTML from now on, and set up one index page for them.

## Question 2
> Why isn't Claude capable of sharing these on a real public website?

## Answer 2
It's mostly a matter of what I've been given access to, not a hard limit on Claude.

- **Sharing controls:** the artifact tools I have can create and update pages, but they can't change who can see them. Making something public is deliberately left to you.
- **No hosting account:** publishing to a real website needs credentials for a host (GitHub Pages, Netlify, your own domain). This project has none connected, and I won't set up accounts or accept sign-ins on your behalf.
- **Public is hard to undo:** once a page is public it can be cached or copied, so it's safest for you to make that call.

If you connect a GitHub repo to the project, a thread could commit the guides to a Pages site, and you'd only have to switch Pages on. Want me to set that up?

## Question 3
> Tell me more about what GitHub Pages is.

## Answer 3
GitHub Pages is a free service from GitHub that turns a repository of HTML files into a public website. You already have a GitHub login (hardyfinn), so you could use it.

**How it works:** you put your HTML files in a repo, switch on Pages in the repo settings, and GitHub serves them at an address like hardyfinn.github.io/vcv-guides. Every time the files change, the site updates by itself.

**Good fit for your guides:**
- Free for public sites, with no server to run.
- Plain HTML works as it is, and an index.html becomes your home page.
- Every edit is saved in history, so you can undo changes.
- You can later point your own domain at it if you want.

**Things to know:**
- Everything in the repo is public, so keep only guides in it, never private notes.
- It's static pages only, which is all a user guide needs.

**What it would take:** you'd create an empty repo (or let me add one to the project), I'd have a thread commit the guides and index page, and you'd switch Pages on in the repo's Settings. Want me to go ahead once you've made the repo?

## Question 4
> That's it, that's interesting. However, some of my modules are clones and some might use names that are copyright, so I can't really push this public.

## Answer 4
That's a sensible call, so I'd keep the guides private. Options that still give you online access:

- **Private artifacts:** each guide and the index page stay private to you, with links that work for you across devices. This is what's set up now, and there's nothing more to do.
- **Private GitHub repo:** keeps the history and editing benefits. Free GitHub Pages only works for public repos, so it wouldn't be a website, but the files would be backed up.
- **Share selectively:** you can share individual artifacts with specific people when you choose, without making anything public.
- **If you ever go public:** publish only the original modules, and give clones neutral names.

Private artifacts are the simplest fit, so I'll carry on with that unless you say otherwise.

## Question 5
> How do I access the private artifacts? Is this just one per GitHub repo? Having a link page with all of my guides would be really helpful.

## Answer 5
Artifacts have nothing to do with GitHub repos. Each one is its own page with its own link, and you can make as many as you like.

**Accessing them:** open a link while signed in to your Claude account. They also appear in your artifact list in Claude, and I can list the ones in this project at any time. Only you can open them unless you share one.

**One page with all your guides:** yes, that's the index page. It's an artifact too, with a link to each guide. Each time a new guide is added, I'd update the index, so you'd bookmark just that one link. Since the links stay private to your account, they only open when you're signed in.

Do you have existing modules whose guides you'd like turned into HTML now, so the index has something in it? If so, tell me which, or upload the PDFs, and I'll start a thread to convert them.

## Question 6
> What model is best for doing this PDF to HTML conversion?

## Answer 6
Sonnet is the best fit. Converting a PDF to HTML is mostly copying and formatting existing content, so it doesn't need Fable or even Opus. Sonnet handles text, tables and headings well and costs far less.

Use Opus only if a guide has complex diagrams or panel layouts that need rebuilding by hand. For a batch of guides, one Sonnet thread per few guides keeps each thread small, and the first one can set the page style that the rest follow. Haiku could work for very plain text-only guides, but Sonnet is the safer default.

## Sources
- No links were given in the answer.
