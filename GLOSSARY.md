# echoVault

Search your own notes and get back the exact text you wrote, with where it came from.

## Language

**Source**:
The app a note lives in, such as Obsidian or Notion.
_Avoid_: Provider, integration, origin

**Note**:
One document inside a Source: a markdown file in Obsidian, a page in Notion.
_Avoid_: Document, file, page (when speaking generally)

**Location**:
Where a Note sits inside its Source: the folder path in Obsidian, the parent pages in Notion.
_Avoid_: Path (when speaking generally), breadcrumb

**Passage**:
A few paragraphs of a Note, stored and returned word for word as one search result. A Note with headings is split at its headings first; a Note without them is split at paragraph breaks.
_Avoid_: Chunk (outside ingest internals), excerpt, snippet, section
