import hashlib
import time

def generate_sign(data, key):
    sorted_keys = sorted(data.keys())
    
    query_string = ""
    for k in sorted_keys:
        v = data[k]
        if v is not None and v != "":  
            if query_string:
                query_string += "&"
            query_string += f"{k}={v}"
    
    query_string += f"&key={key}"
    
    query_string = query_string.replace(" ", "")
    
    sign = hashlib.md5(query_string.encode('utf-8')).hexdigest().upper()
    
    return sign

