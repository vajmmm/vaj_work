import csv

def get_last_hash_from_csv(file_path):
    with open(file_path, 'r') as file:
        reader = csv.DictReader(file)
        rows = list(reader)
        if rows:
            return rows[-1]['Hash']
        return None

def update_hash_list_file(last_hash, file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()

    with open(file_path, 'w') as file:
        found = False
        for line in lines:
            if found:
                file.write(line)
            elif line.strip() == last_hash:
                found = True
                
# 获取最后一个爬取到的 hash 值
last_hash = get_last_hash_from_csv('transaction_data.csv')
print(last_hash)

# 更新 eth_hash.txt 文件
if last_hash:
    update_hash_list_file(last_hash, 'eth_hash.txt')
else:
    print("没有找到最后一个 hash 值，eth_hash.txt 未更新。")