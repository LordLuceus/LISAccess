// Research helper: one persistent MCP connection, so native analysis is not
// thrown away between calls. Full evidence stays in gitignored artifacts.
import fs from 'node:fs/promises';
import path from 'node:path';
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
import readline from 'node:readline';

const [packageRoot, outputDirectory] = process.argv.slice(2);
if (!packageRoot || !outputDirectory) throw new Error('Usage: node rea-session.mjs <installed-rea-package> <evidence-directory>');
const require = createRequire(path.join(path.resolve(packageRoot), 'package.json'));
const { Client } = await import(pathToFileURL(require.resolve('@modelcontextprotocol/client')));
const { StdioClientTransport } = await import(pathToFileURL(require.resolve('@modelcontextprotocol/client/stdio')));
const settings = JSON.parse(await fs.readFile(path.join(process.env.USERPROFILE, '.letta/settings.json'), 'utf8'));
const agentId = process.env.AGENT_ID ?? process.env.LETTA_AGENT_ID;
const agents = settings.agents.filter(a => a.agentId === agentId);
if (agents.length !== 1) throw new Error('Ambiguous agent settings');
const config = agents[0].mcpServers.find(s => s.name === 'rea');
if (!config || config.transport !== 'stdio') throw new Error('REA stdio registration missing');
const output = path.resolve(outputDirectory);
await fs.mkdir(output, { recursive: true });
const transport = new StdioClientTransport({
  command: config.command, args: config.args, cwd: config.cwd,
  env: { ...process.env, ...config.env, REA_GHIDRA_STARTUP_TIMEOUT_MS: '600000' },
  stderr: 'pipe',
});
transport.stderr?.on('data', chunk => process.stderr.write(chunk));
const client = new Client({ name: 'lis-feasibility-research', version: '1' });
let sequence = 0;
let opened = false;
try {
  await client.connect(transport);
  const catalog = await client.listTools();
  await fs.writeFile(path.join(output, 'tool-catalog.json'), JSON.stringify(catalog, null, 2));
  console.log(JSON.stringify({ ready: true, tools: catalog.tools.length, evidenceDirectory: output }));
  const input = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
  for await (const line of input) {
    if (!line.trim()) continue;
    try {
      const request = JSON.parse(line);
      if (request.quit) break;
      if (request.schema) {
        const tool = catalog.tools.find(t => t.name === request.schema);
        if (!tool) throw new Error('Unknown tool');
        console.log(JSON.stringify({ name: tool.name, inputSchema: tool.inputSchema }));
        continue;
      }
      const definition = catalog.tools.find(t => t.name === request.tool);
      if (!definition) throw new Error('Tool not advertised');
      const start = Date.now();
      const result = await client.callTool({ name: request.tool, arguments: request.args ?? {} },
        { timeout: 660000, maxTotalTimeout: 660000, toolDefinition: definition });
      const file = path.join(output, `${String(++sequence).padStart(3, '0')}-${request.tool}.json`);
      await fs.writeFile(file, JSON.stringify({ request, elapsedMs: Date.now() - start, result }, null, 2));
      if (request.tool === 'open_binary' && !result.isError) opened = true;
      if (request.tool === 'close_binary' && !result.isError) opened = false;
      const payload = result.structuredContent ?? JSON.parse(result.content.find(c => c.type === 'text')?.text ?? '{}');
      const value = payload.result ?? payload;
      console.log(JSON.stringify({ file, elapsedMs: Date.now() - start, isError: result.isError ?? false,
        fields: Object.keys(value), preview: JSON.stringify(value).slice(0, 1800) }));
    } catch (error) { console.log(JSON.stringify({ error: String(error) })); }
  }
} finally {
  if (opened) {
    try { await client.callTool({ name: 'close_binary', arguments: {} }); }
    catch (error) { console.error(`Close failed: ${error}`); }
  }
  await client.close();
}
