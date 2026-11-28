---
title: Setting up a Q&A knowledge base
date: 2026-11-28
source: Misc - non work non music · project chat
topic: AI & Claude
tags: [knowledge-base, markdown, note-taking, google-drive, projects]
summary: |
  Keep one plain-text Markdown note per Q&A (title, date, source, tags, summary, full text) in one folder, plus an index page grouped by topic with a search box.
  Projects can't share a project folder, so a Google Drive folder is the best single home that every project with the Drive connector can add to.
---

# Setting up a Q&A knowledge base

## Summary
Keep one plain-text Markdown note per Q&A (title, date, source, tags, summary, full text) in one folder, plus an index page grouped by topic with a search box.
Projects can't share a project folder, so a Google Drive folder is the best single home that every project with the Drive connector can add to.

## Question
> I'm creating lots of questions and answers across multiple projects, and I'm worried I'm going to lose some interesting information. What's the best way to bundle these up so they're searchable and browsable at a really high level?

## Answer
The simplest setup is one plain-text "knowledge base" folder of Markdown notes, one file per Q&A, plus an index file you browse and search.

**How I'd do it**
- **One note per answer:** a short title, the date, the project it came from, 3 to 5 tags, a 2-line summary, then the key answer and its sources. For example, `Hot Lemonade production.md` or `Stem separation state of play.md`.
- **One index page:** a single HTML or Markdown page that groups notes by topic (Music, Tax, Tech, and so on) with one line each. This is the high-level browse view, much like your VCV guide index.
- **Search:** a small search box on that index page, or just your editor or Obsidian, which opens a folder of Markdown files with full-text search, tags and links for free. Nothing is locked in, so you can always move it.
- **Capture habit:** at the end of a good answer, say "save this to the knowledge base" and I'll write the note and update the index. I can also do a one-off sweep of your past threads across this project to backfill the best ones.

**Caveat:** I can only see this project, so for other projects you'd say the same thing there, or paste or export their answers into the same folder.

Want me to start a thread to set up the folder, note template and index page, and backfill what's in this project so far?

## Question 2
> Yeah, let's start that. Are we able to include the whole text of the Q&A session? My questions and answers don't tend to be very long.

## Answer 2
Yes, full text is fine, so I'm starting the knowledge base with complete Q&As in each note.

## Question 3
> Can other projects share this same knowledge base so it's all logged in one place?

## Answer 3
Not directly. Each project has its own shared folder and I can only see this one, so other projects can't write into it automatically.

Three workable options:
- **Google Drive (best for one shared home):** I can see your Drive connector, so the knowledge base could live in a Drive folder that any project with the same connector can read and add to.
- **Manual copy:** other projects save notes in the same format, and you drop them into this folder now and then. Simple, but you do the moving.
- **Git repo:** one repository that every project is given access to, with the notes and index in it.

My suggestion is Drive, if your other projects also have the Drive connector. Do you want me to switch the build thread to a Drive folder, or keep it here for now and move it later? Notes are plain Markdown, so moving is easy either way.

## Sources
- No links were given in the answer.

Related: [[Minimising token usage]] · [[Using one project instead of several]]
