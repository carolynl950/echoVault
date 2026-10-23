# echoVault — Plan

Search your own notes by topic or question and get back the exact passages you wrote, with where they came from. Built to showcase MongoDB Atlas free-tier features for a MongoDB SWE application.

**Budget:** 30 working hours total. Terms (Source, Note, Location, Passage) are defined in `GLOSSARY.md`.

## Decisions

| Area | Decision |
|---|---|
| Audience | Recruiter demo first (public link, sample notes); also runs privately on my real notes. Letting other people connect their own notes is roadmap |
| Sources (v1) | Obsidian (markdown files) and Notion (via API). Results show Source › Location › Note › heading (if any) › line range |
| Output | Retrieval only — verbatim passages with their citation. No LLM-generated answers |
| Search | Hybrid: Atlas Vector Search + Atlas Search (2 of M0's 3 indexes). `$rankFusion` if supported on M0, else manual reciprocal rank fusion via `$unionWith` |
| Embeddings | Atlas Automated Embedding (`autoEmbed`, Voyage). Fallback: call Voyage API directly |
| Ingest | C++ CLI (`echovault ingest`), CMake + mongocxx, hand-written splitter shared by both sources |
| Notion ingest | Same C++ CLI: libcurl for HTTP, `bsoncxx::from_json` for parsing. Blocks → markdown-style lines → shared splitter. Only top-level pages shared with the integration (and their children) are ingested; database rows count as Notes (body only, properties ignored) |
| Web | Next.js (TypeScript) on Vercel, Node driver; single search page + read-only note page |
| Chunking | Split at headings, then split sections > ~400 tokens on paragraph breaks; notes without headings split on paragraph breaks only. Frontmatter excluded from text (tags stored separately); `#` inside code fences is not a heading; `[[wikilinks]]` kept as plain text; images/embeds skipped. Store Location, heading path, line range, tags, mtime |
| Note identity | Obsidian: file path (rename = delete + re-add). Notion: page ID; Location recomputed each run |
| Sync | Manual re-run. Passage identity = content hash: insert new hashes, delete missing ones. Notion: skip fetching pages whose `last_edited_time` is unchanged |
| Links | Demo: read-only note page in the app with the passage highlighted. Personal: `obsidian://` deep links and Notion page URLs |
| Privacy | Two Atlas projects: `demo` (hosted app) and `personal` (local only). Same code, different connection string |
| Demo content | ~30 Obsidian + ~10 Notion (throwaway workspace) fictional notes from one student: classes (biology, US history, intro programming, statistics) and daily learning (cooking, budgeting, books, guitar). Decoy notes sharing words but not meaning. 6 pinned example queries: 2 keyword-wins, 2 semantic-wins, 2 hybrid-wins. Fictional people only |
| Abuse protection | Read-only DB user for Vercel; Vercel built-in rate limiting; 200-char query cap |

## Repo layout

```
ingest/        C++ CLI (CMake)
web/           Next.js app
demo-vault/    fictional Obsidian notes
docs/          PLAN.md, architecture diagram, eval results, indexes/*.json
```

## Hour budget

| Hours | Work |
|---|---|
| 2 | Atlas setup; spike `autoEmbed` + `$rankFusion` on M0 (riskiest first) |
| 3 | C++ toolchain: CMake + mongocxx + libcurl |
| 5 | Ingest: walk vault, split, hash, upsert, delete stale |
| 5 | Notion ingest: API client, blocks → lines, page-ID identity, `last_edited_time` skip |
| 1 | Splitter unit tests |
| 2.5 | Demo content (Obsidian vault + Notion workspace) |
| 3 | Search API (hybrid query) |
| 4 | UI: result cards, citations, read-only note page, deep links, example queries |
| 1.5 | Deploy, read-only user, rate limiting |
| 1.5 | Eval: ~15 queries, hit@5 for vector vs keyword vs hybrid |
| 1.5 | README, diagram, GIF |

**Rules:**
- If C++ toolchain setup exceeds 5h, take a shortcut (prebuilt driver / Docker) rather than cut web time.
- If Notion API ingest exceeds 6h, fall back to Notion's Markdown export run through the Obsidian path.

## Eval

~15 queries written and labeled with their expected Passage right after the demo content exists, before running any search. Hit = expected Passage in top 5 (passage-level, not note-level).

## Definition of done

Live link · hybrid search · verbatim passages with Source/Location/Note/heading/lines · read-only note page on demo · Obsidian + Notion deep links locally · example queries · C++ ingest working on my real Obsidian vault and Notion · README with eval table.

## Cut to v2 (README roadmap)

Connect your own Obsidian/Notion (accounts, per-user data) · live Notion sync · Notion database properties · tag/folder filters · Change Streams live sync · optional cited AI summary · dark-mode polish.

## README outline

1. Pitch + live link
2. 30s GIF
3. Architecture: C++ ingest (Obsidian + Notion) → Atlas (autoEmbed, vector + search indexes) → Next.js
4. Why hybrid search (eval table)
5. Why C++ for ingest
6. Free-tier constraints designed around (3 indexes, two projects)
7. Roadmap
