"""Generate a Lovense session authToken on a trusted machine; never embed the developer key in a viewer."""
import argparse
import getpass
import json
import os
import ssl
import sys
import urllib.error
import urllib.request
import uuid

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uid',help='Opaque ID from your own application; default is a fresh random UUID.')
    args=parser.parse_args()
    developer=os.environ.get('MIKOSTORM_LOVENSE_DEVELOPER_TOKEN') or getpass.getpass('Your Lovense developer token (hidden): ')
    if not developer or len(developer)>4096:
        raise ValueError('A valid developer token is required.')
    uid=args.uid or str(uuid.uuid4())
    if not uid or len(uid)>128:
        raise ValueError('Application user ID must be 1 to 128 characters.')
    body=json.dumps({'token':developer,'uid':uid,'utoken':str(uuid.uuid4())}).encode('utf-8')
    request=urllib.request.Request('https://api.lovense-api.com/api/basicApi/getToken',body,{'Content-Type':'application/json'},method='POST')
    opener=urllib.request.build_opener(NoRedirect(),urllib.request.HTTPSHandler(context=ssl.create_default_context()))
    with opener.open(request,timeout=8) as response:
        raw=response.read(65537)
        if len(raw)>65536:raise ValueError('Unexpected response size.')
        result=json.loads(raw)
    token=result.get('data',{}).get('authToken')
    if result.get('code')!=0 or not isinstance(token,str) or not token or len(token)>4096:
        raise ValueError('Token request rejected; check your Lovense developer configuration.')
    # Explicit CLI output for the owner to paste into the masked viewer field.
    # No files, developer-key logs or shared profile entries are created.
    print(token)

if __name__=='__main__':
    try:main()
    except (ValueError,urllib.error.URLError,OSError,KeyError,TypeError):
        print('Lovense session token could not be created. Check the token, network and developer configuration.',file=sys.stderr)
        sys.exit(1)
