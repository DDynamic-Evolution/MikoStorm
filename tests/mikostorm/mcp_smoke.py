"""Read-only smoke checks for a running newly built viewer. Does not start/build it.
Usage: python tests/mikostorm/mcp_smoke.py --url http://127.0.0.1:13231/mcp
If configured, supply the Bearer token in MIKOSTORM_MCP_TOKEN (never an argument).
"""
import argparse
import http.client
import json
import os
from urllib.parse import urlsplit

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:13231/mcp')
    options = parser.parse_args()
    url = urlsplit(options.url)
    if url.scheme != 'http' or url.hostname not in ('localhost', '127.0.0.1') or url.path != '/mcp' or url.username or url.query or url.fragment:
        parser.error('Use a local HTTP /mcp URL')
    token = os.environ.get('MIKOSTORM_MCP_TOKEN', '')
    session = ''
    def request(method, payload=None, *, use_session=True, path='/mcp', headers=None):
        head = {'Accept': 'application/json', 'Content-Type': 'application/json'}
        if token: head['Authorization'] = 'Bearer ' + token
        if use_session and session:
            head['MCP-Session-Id'] = session
            head['MCP-Protocol-Version'] = '2025-11-25'
        head.update(headers or {})
        connection = http.client.HTTPConnection(url.hostname, url.port or 80, timeout=15)
        try:
            connection.request(method, path, None if payload is None else json.dumps(payload).encode(), head)
            response = connection.getresponse()
            body = response.read()
            return response.status, dict((k.lower(), v) for k, v in response.getheaders()), json.loads(body) if body else None
        finally: connection.close()
    def rpc(rid, method, params=None):
        value = {'jsonrpc': '2.0', 'id': rid, 'method': method}
        if params is not None: value['params'] = params
        status, headers, response = request('POST', value)
        assert status == 200 and response.get('id') == rid, (status, response)
        return headers, response
    assert request('GET', path='/health')[0] == 200
    assert request('GET')[0] == 405
    assert request('POST', {'jsonrpc': '2.0', 'id': 1, 'method': 'tools/list'}, use_session=False)[0] == 400
    response_headers, init = rpc(1, 'initialize', {
        'protocolVersion': '2025-11-25', 'capabilities': {}, 'clientInfo': {'name': 'mikostorm-smoke', 'version': '1'}})
    assert init['result']['protocolVersion'] == '2025-11-25'
    session = response_headers.get('mcp-session-id', '')
    assert session
    try:
        assert request('POST', {'jsonrpc': '2.0', 'method': 'notifications/initialized'})[0] == 202
        _, pong = rpc(9223372036854775807, 'ping')
        assert pong['result'] == {}
        _, tools = rpc('registry', 'tools/list')
        names = {tool['name'] for tool in tools['result']['tools']}
        for tool in tools['result']['tools']:
            assert tool['inputSchema']['type'] == 'object'
            assert tool['outputSchema']['type'] == 'object'
        if 'performance_get' in names:
            _, invalid = rpc(4, 'tools/call', {'name': 'performance_get', 'arguments': {'unexpected': 1}})
            assert invalid['result']['isError']
            _, sample = rpc(5, 'tools/call', {'name': 'performance_get', 'arguments': {}})
            data = sample['result']['structuredContent']
            assert json.loads(sample['result']['content'][0]['text']) == data
            assert isinstance(data['data'], dict)
        assert request('POST', {'jsonrpc': '2.0', 'id': 6, 'method': 'ping'}, headers={'MCP-Session-Id': 'unknown'})[0] == 404
        assert request('POST', {'jsonrpc': '2.0', 'id': 7, 'method': 'ping'}, headers={'MCP-Protocol-Version': '1900-01-01'})[0] == 400
    finally:
        assert request('DELETE')[0] == 204
    print('MCP read-only smoke checks passed; no avatar/chat/inventory actions invoked.')
if __name__ == '__main__': main()
