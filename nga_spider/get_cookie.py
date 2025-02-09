from selenium import webdriver
import time
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait  
from selenium.webdriver.support import expected_conditions as EC 

# 处理Cookie
def format_cookies(cookies):
    cookie_list = [f"{cookie['name']}={cookie['value']}" for cookie in cookies]
    return "; ".join(cookie_list)

def get_cookies(user,passwd):

    browser = webdriver.Chrome()
    browser.get('https://bbs.nga.cn/nuke.php?__lib=login&__act=account&login')
    browser.switch_to.frame(0)

    WebDriverWait(browser,10).until(EC.presence_of_all_elements_located((By.XPATH,"//*[@id='main']/div/div[3]/a[2]")))
    browser.find_element(By.XPATH,"//*[@id='main']/div/div[3]/a[2]").click()

    WebDriverWait(browser,10).until(EC.presence_of_all_elements_located((By.ID,"name")))
    browser.find_element(By.ID,"name").send_keys(user)
    browser.find_element(By.ID,"password").send_keys(passwd)
    browser.find_element(By.XPATH,"//*[@id='main']/div/a[1]").click()

    WebDriverWait(browser,10).until(EC.presence_of_all_elements_located((By.XPATH,"/html/body/div[2]/input")))
    checkcode=input("请输入验证码：")
    browser.find_element(By.XPATH,"/html/body/div[2]/input").send_keys(checkcode)
    browser.find_element(By.XPATH,"/html/body/div[2]/a[1]").click() 

    WebDriverWait(browser, 10).until(EC.alert_is_present())  
    browser.switch_to.alert.accept()

    time.sleep(0.5)
    cookies=browser.get_cookies()
    cookies=format_cookies(cookies)
    print("当前Cookie值为: ",cookies)
    browser.close()
    print("-----------------------------------------------")
    print("登录成功！！！！！！！！！！！！！！！！！！！！！！！！！")
    
    return cookies

if __name__ == '__main__':
    #测试
    user="" #用户名
    passwd="" #密码
    get_cookies(user,passwd)
