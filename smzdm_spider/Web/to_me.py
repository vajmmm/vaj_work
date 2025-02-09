import requests
import datetime

url = "https://push.i-i.me"
file_path = "output.txt"
push_key = "push_key"

def read_file_in_chunks(file_path, chunk_size):
    with open(file_path, 'r', encoding='utf-8') as file:
        while True:
            lines = []
            for _ in range(chunk_size):
                line = file.readline()
                if not line:
                    break
                lines.append(line.strip())
            if not lines:
                break
            yield '\n'.join(lines)

headers = {
    'accept': '*/*',
    'accept-encoding': 'gzip',
    'accept-language': 'zh-CN,zh;q=0.9',
    'content-type': 'application/x-www-form-urlencoded; charset=UTF-8',
    'user-agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.6045.160 Safari/537.36'
}
if __name__ == "__main__":
    for chunk in read_file_in_chunks(file_path, 50):
        payload = {
            'push_key': push_key,
            'title': '物品优惠信息',
            'content': chunk,
            'type': 'text',
            'date': datetime.date.today()  
        }
        response = requests.post(url, headers=headers, data=payload)
        if response.status_code == 200:
            print('发送成功')
        else:
            print('发送失败')