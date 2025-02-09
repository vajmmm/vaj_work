import pymssql

class MSSQLConnection:
    def __init__(self, server, user, password, database, port):
        """初始化数据库连接"""
        self.connection = pymssql.connect(
            server=server,
            user=user,
            spassword=password,
            database=database,
            port=port,
            charset='utf8'
        )

    def truncate_tables(self):
        #重置posts和replies表
        try:
            with self.connection.cursor() as cursor:
                cursor.execute("DELETE FROM replies")
                cursor.execute("DELETE FROM posts")
                cursor.execute("DBCC CHECKIDENT ('posts', RESEED, 0)") #重置自增ID
            self.connection.commit()
            print("-----------------------------------------------")
            print("成功重置原数据库！！！！！")
            print("-----------------------------------------------")
        except Exception as e:
            print(f"Failed to delete tables: {e}")




    def insert_post(self, post_url, title, author, post_time, content):
        """将帖子信息插入到数据库"""
        try:
            with self.connection.cursor() as cursor:
                sql = "INSERT INTO posts (post_url, title, author, post_time, content) VALUES (%s, %s, %s, %s, %s)"
                cursor.execute(sql, (post_url, title, author, post_time, content))
                self.connection.commit()
                cursor.execute("SELECT @@IDENTITY AS id")  # 获取插入帖子的 ID
                post_id = cursor.fetchone()[0]
                print(f"Inserted post with ID: {post_id}")
                return post_id
        except Exception as e:
            print(f"Failed to insert post: {e}")
            return None

    def insert_reply(self, post_id, uid, reply_time, content):
        """将回复信息插入到数据库"""
        try:
            with self.connection.cursor() as cursor:
                sql = "INSERT INTO replies (post_id, uid, reply_time, content) VALUES (%s, %s, %s, %s)"
                cursor.execute(sql, (post_id, uid, reply_time, content))
            self.connection.commit()
            print(f"Inserted reply for post ID: {post_id}")
        except Exception as e:
            print(f"Failed to insert reply: {e}")

    def update_post_content(self, post_id, content):
        """更新帖子的内容"""
        try:
            with self.connection.cursor() as cursor:
                sql = "UPDATE posts SET content=%s WHERE id=%s"
                cursor.execute(sql, (content, post_id))
            self.connection.commit()
            print(f"Updated post content for post ID: {post_id}")
        except Exception as e:
            print(f"Failed to update post content: {e}")

    def close(self):
        """关闭数据库连接"""
        self.connection.close()

if __name__ == '__main__':
    server = 'localhost'  # 替换为你的服务器地址
    user = ''  # 替换为你的用户名
    password = ''  # 替换为你的密码
    database = ''  # 替换为你的数据库名称
    port = 1433
    
    db = MSSQLConnection(server, user, password, database, port)
    db.truncate_tables()
  
    db.close()
