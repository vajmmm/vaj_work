import requests
from lxml import etree
import time
import html
import datetime
from sql_conn import MSSQLConnection  
from get_cookie import get_cookies

# 爬取指定 NGA 论坛版块的帖子信息
def scrape_nga_forum(forum_id, db, pages):
    headers = {
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36 Edg/129.0.0.0',
        'Referer': 'https://bbs.nga.cn/',
        'Connection': 'keep-alive',
        'Cookie': cookies

    }

    for page in range(1, pages + 1):
        url = f'https://bbs.nga.cn/thread.php?fid={forum_id}&page={page}'
        print(f"正在爬取第{page}页...")
        print('------------------------------')

        try:
            response = requests.get(url, headers=headers)
            response.encoding = 'gbk'  # NGA使用GBK编码，utf-8会导致乱码
            if response.status_code != 200:
                print(f"Failed to retrieve page {page}. Status code: {response.status_code}")
                continue

            tree = etree.HTML(response.text)

            links = tree.xpath('//td[@class="c2"]/a/@href')   # 链接
            titles = tree.xpath('//td[@class="c2"]/a/text()')  # 帖子标题
            authors = tree.xpath('//td[@class="c3"]/a/text()')  # 作者
            post_times = tree.xpath('//td[@class="c3"]/span/text()')  # 发布时间

            for href, title, author, post_time in zip(links, titles, authors, post_times):
                if pass_module(author):
                    continue #板块名，忽略这部分

                clean_title = html.unescape(title)
                post_time_standard = datetime.datetime.fromtimestamp(int(post_time)).strftime('%Y-%m-%d %H:%M:%S')
                print(f"帖子链接: {href}, 标题: {clean_title}, 作者: {author}, 发布时间: {post_time_standard}\n")
                time.sleep(1)
                # 插入帖子到数据库
                post_id = db.insert_post(href, clean_title, author, post_time_standard, "内容待抓取")
                if post_id:
                    get_post_details(href, post_id, db)  # 获取并插入详细信息
            time.sleep(5)

        except requests.exceptions.RequestException as e:
            print(f"Error occurred while scraping page {page}: {e}")

# 获取帖子内容以及跟帖内容
def get_post_details(post_link, post_id, db):
    base_url = 'https://bbs.nga.cn'
    post_url = base_url + post_link
    print(f"正在爬取帖子: {post_url}")

    headers = {
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.88 Safari/537.36',
        'Referer': 'https://bbs.nga.cn/',
        'Connection': 'keep-alive',
        'Cookie': cookies
    }

    try:
        response = requests.get(post_url, headers=headers)
        response.encoding = 'gbk'
        if response.status_code != 200:
            print(f"Failed to retrieve post details. Status code: {response.status_code}")
            return

        tree = etree.HTML(response.text)

        
        post_content = tree.xpath('//*[@id="postcontent0"]/text()')
        clean_content = ''.join([html.unescape(text) for text in post_content]).strip()
        print(f"帖子内容: {clean_content}\n")

        
        db.update_post_content(post_id, clean_content) #修改占位符,更新帖子内容

        # 提取回复内容和回复人
        for i in range(0, 15):
            postcontents = tree.xpath(f'//*[@id="postcontent{i}"]/text()')
            postcontdates = tree.xpath(f'//*[@id="postdate{i}"]/text()')
            uids = tree.xpath(f'//*[@id="postauthor{i}"]/@href')
            uid = uids[0].split('uid=')[-1] if uids else "未知UID"

            clean_post_content = ''.join([html.unescape(text) for text in postcontents]).strip()
            time_str = postcontdates[0] if postcontdates else "未知时间"

            if uid == "未知UID" and time_str == "未知时间" and not clean_post_content:
                break

            print(f"回复人UID: {uid}, 回复时间: {time_str}, 回复内容: {clean_post_content}\n")
            db.insert_reply(post_id, uid, time_str, clean_post_content) #插入回帖

            time.sleep(1)
    except requests.exceptions.RequestException as e:
        print(f"Error occurred while scraping post details: {e}")

def pass_module(author):
    if author =="UID:0" or author == "admin":
        return 1
    return 0
    

if __name__ == '__main__':
    server = ''  # 服务器地址
    user = ''  # 用户名
    password = ''  # 密码
    database = ''  # 数据库名称
    port = 1433

    #nga账户
    nga_user="" #用户名
    nga_passwd="" #密码
    cookies=get_cookies(nga_user,nga_passwd) #更新Cookie
    
    db = MSSQLConnection(server, user, password, database, port)
    db.truncate_tables()
    
    forum_id = '616' # NGA 论坛版块 ID
    scrape_nga_forum(forum_id, db, pages=2)

    db.close()
