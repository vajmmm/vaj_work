import random
import time
import pandas as pd
import undetected_chromedriver as uc
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.common.exceptions import TimeoutException, NoSuchElementException
import subprocess


chrome_driver_path = './chromedriver.exe' 

# 持续循环，直到爬取完成
while True: 
    try:
        # 初始化 undetected-chromedriver
        options = uc.ChromeOptions()
        driver = uc.Chrome(options=options, driver_executable_path=chrome_driver_path)

        # 读取交易记录 Hash 列表
        hash_list = []
        with open('eth_hash.txt', 'r') as file:
            hash_list = [line.strip() for line in file if line.strip()]

        if not hash_list:
            print("eth_hash.txt 为空，爬取完成！")
            break

        # 爬取每个交易页面
        for tx_hash in hash_list:
            url = f"https://blockexplorer.one/ethereum/mainnet/tx/{tx_hash}"
            driver.get(url)

            # 等待页面加载并处理 Cloudflare 检测
            WebDriverWait(driver, 30).until(
                EC.presence_of_element_located((By.XPATH, '/html/body/div[2]/div[5]/div[2]/table/tbody/tr[2]/td[1]/a'))
            )
            time.sleep(random.uniform(1, 2))  # 随机延迟
            try:
                not_found_text = driver.find_element(By.XPATH, '/html/body/div[2]/div[5]/div[1]/div[2]/h1').text
                if not_found_text == "Transaction not found! Try again later!":
                    print(f"交易 {tx_hash} 未找到，跳过。")
                    continue
            except NoSuchElementException:
                pass
            # 提取交易信息
            hash = driver.find_element(By.XPATH, '/html/body/div[2]/div[5]/div[2]/table/tbody/tr[2]/td[1]/a').text
            from_address = driver.find_element(By.XPATH, '/html/body/div[2]/div[5]/div[2]/table/tbody/tr[2]/td[2]/div/a').text
            to_address = driver.find_element(By.XPATH, '/html/body/div[2]/div[5]/div[2]/table/tbody/tr[2]/td[3]/div/a').text
            amount = driver.find_element(By.XPATH, '/html/body/div[2]/div[5]/div[2]/table/tbody/tr[2]/td[4]/b').text
            timestamp = driver.find_element(By.XPATH, '/html/body/div[2]/div[5]/div[2]/table/tbody/tr[2]/td[5]/span').text
            fee = driver.find_element(By.XPATH, '/html/body/div[2]/div[5]/div[2]/table/tbody/tr[2]/td[6]/div').text

            # 转换时间戳为 Unix 时间戳
            unix_timestamp = int(time.mktime(time.strptime(timestamp, "%d/%m/%Y - %H:%M:%S")))

            print(f"Hash: {hash}")
            print(f"From: {from_address}")
            print(f"To: {to_address}")
            print(f"Transaction Amount: {amount}")
            print(f"Timestamp: {timestamp}")
            print(f"Fee: {fee}")
            print(f"Unix Timestamp: {unix_timestamp}")
            print()

            # 创建单条记录的 DataFrame
            df = pd.DataFrame([{
                "Hash": tx_hash,
                "From": from_address,
                "To": to_address,
                "Transaction Amount": amount,
                "TimeStamp": unix_timestamp,
                "Fee": fee
            }])

            # 追加到 CSV 文件
            df.to_csv('transaction_data.csv', mode='a', header=not pd.io.common.file_exists('transaction_data.csv'), index=False,
                      columns=["Hash", "From", "To", "Transaction Amount", "TimeStamp", "Fee"])

        driver.quit()
        print('-----------------------------------')
        print("爬取完成！")
        print('-----------------------------------')

        # 检查 eth_hash.txt 是否为空
        with open('eth_hash.txt', 'r') as file:
            if file.read().strip():
                # 调用 update.py 更新 eth_hash.txt
                subprocess.run(['python', 'update.py'])
            else:
                print("eth_hash.txt 为空，结束循环。")
                break

    except (TimeoutException, NoSuchElementException) as e:
        print(f"发生错误：{e}")
        driver.quit()
        print("调用 update.py 更新后重新执行...")
        subprocess.run(['python', 'update.py'])
        continue