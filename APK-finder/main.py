import requests
import json
import time
import random
from lxml import html
import hashlib
import os
import csv
from tqdm import tqdm
from concurrent.futures import ThreadPoolExecutor, as_completed
from androguard.core.apk import APK
from androguard.util import set_log
import hashlib
import os
import zipfile
import logging
set_log("ERROR")

def is_valid_apk(filename):
    try:
        with zipfile.ZipFile(filename, 'r') as zip_ref:
            # 尝试读取文件内容，若成功说明是有效的APK
            zip_ref.testzip()
        return True
    except (zipfile.BadZipFile, FileNotFoundError) as e:
        print(f"文件 {filename} 不是有效的APK文件：{e}")
        return False

def download_chunk(url, start, end, filename, chunk_num, pbar, retries=3):
    headers = {'Range': f'bytes={start}-{end}'}
    chunk_filename = f"{filename}.part{chunk_num}"
    
    for attempt in range(retries):
        try:
            response = requests.get(url, headers=headers, stream=True, timeout=10)
            if response.status_code in [200, 206]:
                with open(chunk_filename, 'wb') as f:
                    for chunk in response.iter_content(chunk_size=4096):
                        f.write(chunk)
                        pbar.update(len(chunk))
                return chunk_filename
            else:
                print(f"下载失败，状态码: {response.status_code}")
        except requests.exceptions.RequestException as e:
            print(f"下载失败，尝试重试 ({attempt + 1}/{retries}) 次: {e}")
    
    return None

def download_file(url, filename, retries=3, num_threads=8):
    
    if not os.path.exists('APK'):
        os.makedirs('APK')
    
    filepath = os.path.join('APK', filename)
    
    # 获取文件大小并计算块的大小
    try:
        response = requests.head(url, timeout=10)
        total_size = int(response.headers.get('content-length', 0))
    except requests.exceptions.RequestException as e:
        print(f"获取文件大小失败: {e}")
        return None
    
    
    chunk_size = total_size // num_threads
    ranges = [(i * chunk_size, (i + 1) * chunk_size - 1) for i in range(num_threads)]
    ranges[-1] = (ranges[-1][0], total_size - 1)  # 最后一个块的结束位置
    
    with tqdm(total=total_size, unit='iB', unit_scale=True, desc=filename, unit_divisor=1024) as pbar:
        with ThreadPoolExecutor(max_workers=num_threads) as executor:
            futures = [executor.submit(download_chunk, url, start, end, filepath, i, pbar) for i, (start, end) in enumerate(ranges)]
            chunk_files = [future.result() for future in as_completed(futures)]
    
    # 检查下载文件是否全部成功
    if None in chunk_files:
        print(f"下载失败: {filename}")
        return None
    
    # 合并文件
    try:
        with open(filepath, 'wb') as f:
            for chunk_file in sorted(chunk_files, key=lambda x: int(x.split('.part')[1])):  # 确保按照顺序合并
                with open(chunk_file, 'rb') as cf:
                    f.write(cf.read())
                os.remove(chunk_file)
    except Exception as e:
        print(f"合并文件失败: {e}")
        return None

    # 校验下载后的文件是否为有效的APK
    if not is_valid_apk(filepath):
        print(f"文件 {filepath} 合并后无效！")
        os.remove(filepath)  
        return None

    # 验证下载文件大小
    try:
        response = requests.head(url, timeout=10)
        expected_size = int(response.headers.get('content-length', 0))
        actual_size = os.path.getsize(filepath)
        
        if actual_size != expected_size:
            print(f"文件 {filepath} 大小不匹配！预期大小: {expected_size}, 实际大小: {actual_size}")
            os.remove(filepath) 
            return None
    except requests.exceptions.RequestException as e:
        print(f"检查文件大小失败: {e}")
        return None
    
    return filepath

def calculate_md5(filename):
    hash_md5 = hashlib.md5()
    with open(filename, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def fetch_app_detail(package_name):
    proxies = {
        "http": None,
        "https": None,
    }
    detail_url = 'https://sj.qq.com/appdetail/{}'.format(package_name)
    response = requests.get(detail_url, proxies=proxies)
    
    if response.status_code == 200:
        response.encoding = response.apparent_encoding
        tree = html.fromstring(response.text)
        
        for i in range(6, 0, -1):
            xpath = f'//*[@id="__next"]/div/main/div[3]/div/div[1]/div/div[{i}]/p'
            data = tree.xpath(xpath)
            if data:
                return data[0].text_content().replace('\n', ' ').replace('\r', ' ')
        
        return "此应用无简介"
    else:
        return f"请求失败，状态码: {response.status_code}"

def process_app(item):
    package_name = item.get('pkg_name')
    app_detail = fetch_app_detail(package_name)

    download_url = item.get('download_url')
    filename = "{}.apk".format(package_name)
    filepath = os.path.join('APK', filename)
     # 检查文件是否已经存在
    if os.path.exists(filepath):
        print(f"文件 {filename} 已存在，直接提取数据")
        downloaded_file = filepath
    else:
        downloaded_file = download_file(download_url, filename)
        
    if downloaded_file:
        md5_hash = calculate_md5(downloaded_file)
        try:
            # 使用 androguard 提取 APK 信息
            Apk = APK(downloaded_file)
            version_name = Apk.get_androidversion_name()
            min_sdk_version = Apk.get_min_sdk_version()
        except Exception as e:
            print(f"提取 APK 信息失败: {e}")
            version_name = "未知"
            min_sdk_version = "未知"
    else:
        md5_hash = "下载失败"
        version_name = "未知"
        min_sdk_version = "未知"

    app_info = {
        'name': item.get('name'),
        'package_name': package_name,
        'average_rating': round(float(item.get('average_rating', 0)), 1),
        'developer': item.get('developer'),
        'download_url': download_url,
        'description': app_detail,
        'md5': md5_hash,
        'version_name': version_name,
        'min_sdk_version': min_sdk_version
    }

    with open(csv_file, 'a', newline='', encoding='utf-8') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=csv_columns)
        writer.writerow(app_info)

    print("Name: {}".format(app_info['name']))
    print("Package Name: {}".format(app_info['package_name']))
    print("Average Rating: {}".format(app_info['average_rating']))
    print("Developer: {}".format(app_info['developer']))
    print("Download URL: {}".format(app_info['download_url']))
    print("Description: {}".format(app_info['description']))
    print("MD5: {}".format(app_info['md5']))
    print("Version Name: {}".format(app_info['version_name']))
    print("Minimum SDK Version: {}".format(app_info['min_sdk_version']))

url = 'https://yybadaccess.3g.qq.com/v2/dynamicard_yybhome'

headers = {
    'Host': 'yybadaccess.3g.qq.com',
    'Connection': 'keep-alive',
    'Content-Length': '441',
    'sec-ch-ua-platform': '"Android"',
    'User-Agent': 'Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Mobile Safari/537.36 Edg/131.0.0.0',
    'sec-ch-ua': '"Microsoft Edge";v="131", "Chromium";v="131", "Not_A Brand";v="24"',
    'Content-Type': 'text/plain;charset=UTF-8',
    'sec-ch-ua-mobile': '?1',
    'Accept': '*/*',
    'Origin': 'https://sj.qq.com',
    'Sec-Fetch-Site': 'same-site',
    'Sec-Fetch-Mode': 'cors',
    'Sec-Fetch-Dest': 'empty',
    'Referer': 'https://sj.qq.com/',
    'Accept-Encoding': 'gzip, deflate, br, zstd',
    'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6'
}

proxies = {
    "http": None,
    "https": None,
}

limit = 25
total = 500

csv_file = 'apps_info.csv'
csv_columns = ['name', 'package_name', 'average_rating', 'developer', 'download_url', 'description', 'md5', 'version_name', 'min_sdk_version']

with open(csv_file, 'w', newline='', encoding='utf-8') as csvfile:
    writer = csv.DictWriter(csvfile, fieldnames=csv_columns)
    writer.writeheader()

for offset in range(0,22):
    data = {
        "head": {
            "cmd": "dynamicard_yybhome",
            "authInfo": {
                "businessId": "AuthName"
            },
            "deviceInfo": {
                "platform": 2
            },
            "userInfo": {
                "guid": "3f867617-dc39-47a6-863d-060c281a685a"
            },
            "expSceneIds": "",
            "hostAppInfo": {
                "scene": "app_center"
            }
        },
        "body": {
            "bid": "yybhome",
            "offset": 0,
            "size": 10,
            "preview": False,
            "listS": {
                "region": {
                    "repStr": ["CN"]
                },
                "cate_alias": {
                    "repStr": ["all"]
                }
            },
            "listI": {
                "limit": {
                    "repInt": [limit]
                },
                "offset": {
                    "repInt": [offset]
                }
            },
            "layout": "YYB_HOME_APP_LIBRARY_LIST"
        }
    }

    response = requests.post(url, headers=headers, json=data, proxies=proxies)
    print("Status Code:", response.status_code)

    response_json = response.json()

    components = response_json.get('data', {}).get('components', [])
    item_data_list = []
    for component in components:
        item_data_list.extend(component.get('data', {}).get('itemData', []))
    
    for item in item_data_list:
        process_app(item)

    time.sleep(random.uniform(1, 3))

print("应用信息已保存到 {}".format(csv_file))
