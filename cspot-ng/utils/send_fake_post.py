# make a python script that sends a post request
# POST request to path: /spotify_info
# Headers:
# Accept-Encoding: gzip
# Connection: keep-alive
# Content-Length: 626
# Content-Type: application/x-www-form-urlencoded
# Host: 192.168.1.143:7864
# Keep-Alive: 0
# User-Agent: Spotify/9.0.30 iOS/18.3.2 (iPhone17,5)
# Body: action=addUser&userName=pablogs9&blob=2kpkjNkZC%2FnwPPGVp4ffVNI8ODiUG296NYeJEtPyUUgz5FM4KCHTHw4bFVGHTq7maJcufaxfPWy%2B77YOfz6IyJOta%2FQKfqWYtcypE0HX%2BJdgPRJxNxvDgmULyAF5Ynvf6ng1MvPjhlro1x0RMhmzNqTC%2BByng8VgXn0wsF9Iqz4NN2QRYIlMLPZv%2BQBfYofGPWl9bGwkLDjIVBquha6pkQrRSWA1TFqGo9Ap6QddxJuPUSu34gh1JPpYu5FaWn9Kgf%2BMYp3Vj9eoIcgHOu8u2w%3D%3D&clientKey=5ZtpngYFC8%2FUz3VSkwELcYrJ7%2BjKqO0P7AuzNxjFxWeifnlGA4K%2FHnbb%2F2MKRFuUiKVMOUa055uEit2HXMgjEShUuTZP9SMqqlItyhRPfx5r%2Fy4lig1IW65OD0iqQRos&tokenType=default&loginId=d35bee7420ae64cae1c11e345ca9e411&deviceName=iPhone&deviceId=b28efed45e08a348a5f3fcd16eaf2de5c0756ec4&version=2.7.1


import requests
import json
import base64
import time
import hashlib

def send_post_request():
    url = "http://localhost:7864/spotify_info"

    headers = {
        "Accept-Encoding": "gzip",
        "Connection": "keep-alive",
        "Content-Type": "application/x-www-form-urlencoded",
        "Host": "192.168.1.143:7864",
        "Keep-Alive": "0",
        "User-Agent": "Spotify/9.0.30 iOS/18.3.2 (iPhone17,5)"
    }

    body = "action=addUser&userName=pablogs9&blob=%2B1pv1yJHm9FF0eBOaxoJlij9g8F8e43UmYX5x5CJUYJP28QZII9bW7y839MIMeyE96GpdxPQ5svzw8UeViDiK5cki1rC9xhvFj%2BhFgetP3qWUxEh1WWAsp1MugYA11uH%2FATuV%2FnA2iZmWJzSjwCOt3vj%2BIpmoO4fjcbLVuTCz1DRoUlZpOHpuYwMTnwlIzC1H3c%2BeN73hgglS%2BSu4R6PWYhVSNgvctUqmpEb4y6mR2AXtHKJqqdTCzPChUQtFxAYUzRyIukIKpbPzslgXgEhBw%3D%3D&clientKey=BDlMSzFVQ0d82gJxfyNwCwN485v81yyUqz2AcmnfEmJt0C%2BfvRBYNoON6TXAlGoQxJVa3NCBUtMv8jIo%2BnMRGnVe4qTwjZs99yyaoT3IAMSVukFSD11waF54KBh60bzz&tokenType=default&loginId=7e8c5a8d177ead0fced331b8613cd2a7&deviceName=iPhone&deviceId=b28efed45e08a348a5f3fcd16eaf2de5c0756ec4&version=2.7.1"

    try:
        # Send POST request
        response = requests.post(url, headers=headers, data=body)

        # Print response status and headers
        print(f"Status Code: {response.status_code}")
        print("Response Headers:")
        for key, value in response.headers.items():
            print(f"{key}: {value}")

        # Try to parse as JSON if possible
        try:
            response_data = response.json()
            print("\nResponse JSON:")
            print(json.dumps(response_data, indent=2))
        except json.JSONDecodeError:
            # If not JSON, print text
            print("\nResponse Text:")
            print(response.text)

    except Exception as e:
        print(f"Error occurred: {e}")

send_post_request()