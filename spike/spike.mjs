// Spike: does this Atlas cluster support autoEmbed and $rankFusion?
// Run: node --env-file=../.env spike.mjs
import { MongoClient } from "mongodb";

const client = new MongoClient(process.env.MONGODB_URI);
const db = client.db("echovault_spike");
const col = db.collection("passages");

const docs = [
  { text: "The mitochondria is the powerhouse of the cell; cellular respiration makes ATP." },
  { text: "After a big lunch I always get sleepy, probably blood sugar crashing." },
  { text: "The Treaty of Ghent ended the War of 1812 in December 1814." },
  { text: "Budget spreadsheet: put rent in cell B2 and groceries in cell B3." },
  { text: "Practiced the G to C chord change on guitar for twenty minutes." },
  { text: "Sourdough starter needs feeding every 12 hours at room temperature." },
];

const results = {};
const step = async (name, fn) => {
  try {
    const out = await fn();
    results[name] = "PASS";
    console.log(`✅ ${name}`, out ?? "");
  } catch (e) {
    results[name] = `FAIL: ${e.message}`;
    console.log(`❌ ${name}: ${e.message}`);
  }
};

async function waitQueryable(name, timeoutMs = 300_000) {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    const [idx] = await col.listSearchIndexes(name).toArray();
    if (idx?.status === "FAILED") throw new Error(`index FAILED: ${JSON.stringify(idx)}`);
    if (idx?.queryable) return idx.status;
    await new Promise((r) => setTimeout(r, 5000));
  }
  throw new Error("timed out waiting for index");
}

await client.connect();

await step("server version", async () => {
  const info = await db.admin().command({ buildInfo: 1 });
  return info.version;
});

// Don't drop the collection: on M0, search indexes on a dropped collection
// keep counting toward the 3-index cap until their async deletion finishes.
await step("reset + insert sample docs", async () => {
  await col.deleteMany({});
  await col.insertMany(docs);
  return `${docs.length} docs`;
});

const existing = new Set((await col.listSearchIndexes().toArray()).map((i) => i.name));
const ensureIndex = async (spec) => (existing.has(spec.name) ? "already exists" : col.createSearchIndex(spec));

await step("create autoEmbed vector index", () =>
  ensureIndex({
    name: "passages_vector",
    type: "vectorSearch",
    definition: {
      fields: [{ type: "autoEmbed", modality: "text", path: "text", model: "voyage-4-lite" }],
    },
  }),
);

await step("create text search index", () =>
  ensureIndex({
    name: "passages_text",
    definition: { mappings: { dynamic: false, fields: { text: { type: "string" } } } },
  }),
);

await step("vector index queryable", () => waitQueryable("passages_vector"));
await step("text index queryable", () => waitQueryable("passages_text"));

// A query against a missing index silently returns [], so an empty result is a failure here.
const nonEmpty = (arr) => {
  if (arr.length === 0) throw new Error("no results");
  return arr;
};

const q = "why do I feel tired after eating";
const vectorStage = { $vectorSearch: { index: "passages_vector", path: "text", query: q, numCandidates: 50, limit: 5 } };
const searchStage = { $search: { index: "passages_text", text: { query: q, path: "text" } } };

await step("$vectorSearch with query text", async () =>
  nonEmpty((await col.aggregate([vectorStage, { $project: { _id: 0, text: 1 } }]).toArray()).map((d) => d.text.slice(0, 50))),
);

await step("$search keyword", async () =>
  nonEmpty((await col.aggregate([searchStage, { $limit: 5 }, { $project: { _id: 0, text: 1 } }]).toArray()).map((d) => d.text.slice(0, 50))),
);

await step("$rankFusion", async () =>
  nonEmpty(
    (
      await col
        .aggregate([
          { $rankFusion: { input: { pipelines: { vector: [vectorStage], keyword: [searchStage, { $limit: 5 }] } } } },
          { $limit: 5 },
          { $project: { _id: 0, text: 1 } },
        ])
        .toArray()
    ).map((d) => d.text.slice(0, 50)),
  ),
);

console.log("\nSUMMARY", JSON.stringify(results, null, 2));
await client.close();
