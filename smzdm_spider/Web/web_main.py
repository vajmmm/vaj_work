import requests
from bs4 import BeautifulSoup
import re
from lxml import etree
import time
import os


URL="https://faxian.smzdm.com/h4s0t0f0c0p"
def getURL(url):
    head = {
        "user-agent":"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36 Edg/128.0.0.0",
        "cookie":"__ckguid=pRv3PuO4S6PkAhtrprVKx2; device_id=2130706433172558959374061123daeeaed1d2326f83f0a917df219b7d; homepage_sug=c; r_sort_type=score; smzdm_ec=06; smzdm_ea=02; smzdm_user_source=8A5B5331E03CDE84B360976D5B42B737; _zdmA.uid=ZDMA.ZNMLBW1b8.1726146207.2419200; Hm_lvt_9b7ac3d38f30fe89ff0b8a0546904e58=1725960212,1725965806,1726146207,1726193908; HMACCOUNT=EDB74CA8383EB2E0; hiplmt_33623244c62a33b5ea597aa2f1e3f443_silent_token=nyzb4V52RuMNcCGuYISbzmPHQDom%2FPxu6fVm4bPS1WRKWxeIsAson6Vzy3kW3f6Q1bcMPtmPw88%2FfoKfPjtymbVUJxyl33fHAYU%2FWpYDivgtGjKuNZGXffcR4phNNEQx4oZtlCyud2RZU31AEg%3D%3D; sensorsdata2015jssdkcross=%7B%22distinct_id%22%3A%22191c527a41ae51-06866d2473769fc-4c657b58-1821369-191c527a41b75b%22%2C%22first_id%22%3A%22%22%2C%22props%22%3A%7B%22%24latest_traffic_source_type%22%3A%22%E7%9B%B4%E6%8E%A5%E6%B5%81%E9%87%8F%22%2C%22%24latest_search_keyword%22%3A%22%E6%9C%AA%E5%8F%96%E5%88%B0%E5%80%BC_%E7%9B%B4%E6%8E%A5%E6%89%93%E5%BC%80%22%2C%22%24latest_referrer%22%3A%22%22%2C%22%24latest_landing_page%22%3A%22https%3A%2F%2Ffaxian.smzdm.com%2Fh4s0t0f0c0p5%2F%22%7D%2C%22identities%22%3A%22eyIkaWRlbnRpdHlfY29va2llX2lkIjoiMTkxYzUyN2E0MWFlNTEtMDY4NjZkMjQ3Mzc2OWZjLTRjNjU3YjU4LTE4MjEzNjktMTkxYzUyN2E0MWI3NWIifQ%3D%3D%22%2C%22history_login_id%22%3A%7B%22name%22%3A%22%22%2C%22value%22%3A%22%22%7D%2C%22%24device_id%22%3A%22191c527a41ae51-06866d2473769fc-4c657b58-1821369-191c527a41b75b%22%7D; Hm_lpvt_9b7ac3d38f30fe89ff0b8a0546904e58=1726196152"
        }
    response=requests.get(url,headers=head)
    
    if response.status_code != 200:
        raise Exception()
    return response


def get_target(r):
    r.content.decode('utf-8')
    tree = etree.HTML(r.content)
    results = tree.xpath("//*[@id='feed-main-list']/li[@class='J_feed_za']/div")
    for result in results:
        full_text = result.xpath('string()')
        time=get_time(full_text)
        divs = result.xpath("./div")

        user_divs = divs[4]
        user_div = user_divs[0]
        appraise = user_div.xpath("./span/a/span/span")[0]
        comment = user_div.xpath("./a/text()")
        
        if not( appraise.text.endswith("k") or comment[0].endswith("k")):
            if int(appraise.text) >= 20 and int(comment[0])>=20:
                appraise=int(appraise.text)

                if get_price(full_text):
                    price = get_price(full_text)
                else:
                    price = divs[1].text

                brief = result.xpath("./h5/a")[0]

                print("链接：",brief.get('href'),"发布时间：",time,"价格：",price,"简介：",brief.text.strip())
                print("点值率：",appraise,"评论数：",comment[0])

                with open('output.txt', 'a', encoding='utf-8') as f:  # 使用'a'模式，表示追加内容到文件末尾
                    f.write(f"链接：{brief.get('href')} | 发布时间：{time} | 价格：{price} | 简介：{brief.text.strip()} | 点值率：{appraise} | 评论数：{comment[0]}\n")
                
            else :
                return
        else :
          
            if appraise.text.endswith("k"):
               
                appraise = float((appraise.text)[:-1]) * 1000
            else:
                appraise = int(appraise.text)
            if comment[0].endswith("k"):
                comment[0] = float(comment[0][:-1])*1000
            else:
                comment[0] = int(comment[0])
            
            if int(appraise) >= 20 and comment[0] >= 20:

                if get_price(full_text)!= []:
                    price = get_price(full_text)
                else:
                    price = divs[1].text

                time_divs = divs[2]
                time_div = time_divs[1]
                brief = result.xpath("./h5/a")[0]


                print("链接：",brief.get('href'),"发布时间：",time,"价格：",price,"简介：",brief.text.strip())
                print("点值率：",appraise,"评论数：",comment[0])
                
                with open('output.txt', 'a', encoding='utf-8') as f:  # 使用'a'模式，表示追加内容到文件末尾
                    f.write(f"链接：{brief.get('href')} | 发布时间：{time} | 价格：{price} | 简介：{brief.text.strip()} | 点值率：{appraise} | 评论数：{comment[0]}\n")
               
                
        


def get_targets():
    global URL
    for i in range(1,7):
        url = URL+str(i)
        r=getURL(url)
        get_target(r)
        

def get_time(text):
    pattern = r'^(?:(?:[0-9]{3}[-\s]*){11}$'

    matches = re.findall(pattern,text)
    time = [match[0] if match[0] else match[1] for match in matches]
    #print(time)
    return time[0]

def get_price(text):
    #print(text)
    pattern = r'\s\d+(?:\.\d+)?元(?:包邮)?(?:（.*?）)?\s'
    price = re.findall(pattern,text)
    #print(price)
    if not price:
        return 
    else:
        return price[0]
    

def get_time(text):
    pattern = r'(\d{2}:\d{2})|(\d{2}-\d{2} \d{2}:\d{2})'

    matches = re.findall(pattern,text)
    time = [match[0] if match[0] else match[1] for match in matches]
    #print(time)
    return time[0]


if __name__ == "__main__":
    
    get_targets()
    os.system("python -u d:\大三作业\Python开发\实验1\to_me.py'")

