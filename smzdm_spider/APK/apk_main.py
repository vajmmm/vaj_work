import requests
import time
import hashlib
import json
import os

# 获取当前时间戳（毫秒）
def get_current_time_millis():
    return int(time.time()) * 1000

# 生成 sign
def generate_sign(params, key):
    sorted_keys = sorted(params.keys())
    param_str = '&'.join(f"{k}={params[k]}" for k in sorted_keys if params[k] != '')
    param_str += f"&key={key}"
    return hashlib.md5(param_str.encode()).hexdigest().upper()

# 构建请求参数
def build_params(base_params, additional_params):
    params = base_params.copy()
    params.update(additional_params)
    return params

# 发送请求并解析响应
def fetch_json(url):
    response = requests.get(url,verify=True)
    if response.status_code == 200:
        return response.json()
    else:
        print("请求失败，状态码：", response.status_code)
        return None

# 获取商品详情
def get_article_details(article_id, key):
    current_time = get_current_time_millis()
    params = {
        #'h5hash': '',
        'imgmode': '0',
        #'hashcode': '',
        'zhuanzai_ab': 'a',
        'weixin': '0',
        'f': 'android',
        'v': '11.0.33',
        'time': str(current_time),
        'basic_v': '0'
    }
    params['sign'] = generate_sign(params, key)

    url = (
        f"https://haojia-api.smzdm.com/detail/{article_id}?"
        #f"h5hash={params['h5hash']}&"
        f"imgmode={params['imgmode']}&"
        #f"hashcode={params['hashcode']}&"
        f"zhuanzai_ab={params['zhuanzai_ab']}&"
        f"weixin={params['weixin']}&"
        f"f={params['f']}&"
        f"v={params['v']}&"
        f"time={params['time']}&"
        f"basic_v={params['basic_v']}&"
        f"sign={params['sign']}"
    )
    #print(url)
    json_data = fetch_json(url)
    if json_data and 'data' in json_data:
        return json_data['data']
    else:
        print("数据中没有找到 'data' 字段或响应无效")
        return None
    
# 检验是否满足标准
def inspection(comment,worthrate):
    if comment < 5 or worthrate < 60 :
        return False
    return True


    
# 分隔符
def separate(n):
    print("----------------------------------------")
    print("第",n,"组数据")
    print("----------------------------------------")


def main():
    key = 'apr1$AwP!wRRT$gJ/q.X24poeBInlUJC'
    base_params = {
        'limit': '20',
        'offset': '0',
        'tab': '1',
        'tab_id': '67',
        'slot': '4',
        'channel_id': '',
        'sub_tab': '0',
        'exclude_article_ids': 'null',
        'rank_feed': '',
        'zhuanzai_ab': 'a',
        'weixin': '0',
        'f': 'android',
        'v': '11.0.33',
        'time': str(get_current_time_millis()),
        'basic_v': '0'
    }
   
    base_params['sign'] = generate_sign(base_params, key)

  
    list_url = (
        "https://haojia-api.smzdm.com/ranking_list/articles?"
        f"limit={base_params['limit']}&"
        f"offset={base_params['offset']}&"
        f"tab={base_params['tab']}&"
        f"tab_id={base_params['tab_id']}&"
        f"slot={base_params['slot']}&"
        f"channel_id={base_params['channel_id']}&"
        f"sub_tab={base_params['sub_tab']}&"
        f"exclude_article_ids={base_params['exclude_article_ids']}&"
        f"rank_feed={base_params['rank_feed']}&"
        f"zhuanzai_ab={base_params['zhuanzai_ab']}&"
        f"weixin={base_params['weixin']}&"
        f"f={base_params['f']}&"
        f"v={base_params['v']}&"
        f"time={base_params['time']}&"
        f"basic_v={base_params['basic_v']}&"
        f"sign={base_params['sign']}"
    )

    file_path = os.path.join(os.getcwd(), 'goods_content.txt')
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write('')  
    

    data = fetch_json(list_url)
    
    if data and 'data' in data:
        links_info =[
                        {
                            'link': row['redirect_data']['link'],
                            'article_worthy': float(row['article_worthy']),
                            'article_unworthy': float(row['article_unworthy']),
                            'article_comment': int(row['article_comment'])
                        }
                        for row in data['data']['rows'] if '?' not in row['redirect_data']['link']
                    ]
        index=1
        for link in links_info:
            separate(index)
            index+=1

            article_id = link['link'].split('/')[-1]
            article_details = get_article_details(article_id, key)
            article_comment = link['article_comment']
            article_worthyrate = round((link['article_worthy']/(link['article_worthy']+link['article_unworthy'])),2)*100
            
            if not inspection(article_comment,article_worthyrate):
                print("评论或点值率不满足要求，忽略本组数据！")
                continue
            
            if article_details:
                article_info = (
                    f"发布日期: {article_details['article_pubdate']}\n"
                    f"商品页面: {article_details['article_url']}\n"
                    f"商品标题: {article_details['article_title']}\n"
                    f"商品价格: {article_details['article_price']}\n"
                    f"评论数: {article_comment}\n"
                    f"点值率: {article_worthyrate}%\n\n"
                )
                
                print(article_info)
                
                with open(file_path, 'a', encoding='utf-8') as f:
                    f.write(article_info)
            
                
    else:
        print("无法获取数据")

if __name__ == "__main__":
    main()
    #os.system('python3 ./to_me.py')

