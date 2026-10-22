import asyncio
from pathlib import Path
import sys
import os
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

async def main():
    root = Path(__file__).resolve().parent.parent
    server = StdioServerParameters(command=sys.executable,
        args=['-m', 'code_review_graph', 'serve', '--repo', str(root)], cwd=str(root))
    async with stdio_client(server) as (read, write):
        async with ClientSession(read, write) as session:
            result = await session.initialize()
            tools = await session.list_tools()
            print('MCP handshake OK:', result.serverInfo.name)
            print('Tools:', len(tools.tools))
            print(', '.join(t.name for t in tools.tools[:12]))

asyncio.run(asyncio.wait_for(main(), timeout=40))
