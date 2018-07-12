#!/usr/bin/env python
# -*- coding:utf-8 -*-
import sys
import tornado.httpclient
import json
import time
import re

class Server(object):
    def __init__(self):
        self.username = 'xxx'
        self.passwd = 'xxx'
        self.invite_code = 'xxx'
        self.eobzz_token = None
        self.phone_num = None

    def get_eobzz_token(self):
        print '=========================get_eobzz_token========================='
        http_client = tornado.httpclient.HTTPClient()
        m_url = "http://api.eobzz.com/httpApi.do?action=loginIn&uid=%s&pwd=%s" % (self.username, self.passwd)
        print m_url
        request = tornado.httpclient.HTTPRequest(url= m_url, method='GET', connect_timeout=10, request_timeout=60)
        response = http_client.fetch(request)
        print response.body
        tmp = response.body.split('|')
        self.eobzz_token = tmp[1]

    def get_phone_num(self):
        print '\n=========================get_phone_num========================='
        http_client = tornado.httpclient.HTTPClient()
        m_url = 'http://api.eobzz.com/httpApi.do?action=getMobilenum&pid=36848&uid=%s&token=%s&mobile=&size=1' % (self.username, self.eobzz_token)
        print m_url
        request = tornado.httpclient.HTTPRequest(url= m_url, method='GET', connect_timeout=10, request_timeout=60)
        response = http_client.fetch(request)
        print response.body
        tmp = response.body.split('|')
        self.phone_num = tmp[0]
        self.eobzz_token = tmp[1]

    def send_sms_code(self):
        print '\n=========================send_sms_code========================='
        send_data = {}
        http_client = tornado.httpclient.HTTPClient()
        send_data['phone'] = self.phone_num
        _json_data = json.dumps(send_data)
        http_body = _json_data
        header_data = {}
        header_data['Accept-Language'] = 'zh-Hans-CN;q=1.0'
        header_data['Content-Type'] = 'application/json'

        request = tornado.httpclient.HTTPRequest(url='http://api.api.chongdingdahui.com/user/requestSmsCode', method='POST', connect_timeout=10, request_timeout=60, body=http_body, headers = header_data, user_agent='LiveTrivia/1.0.5 (com.chongdingdahui.app; build:0.2.0; iOS 11.2.2) Alamofire/4.6.0')
        response = http_client.fetch(request)
        print response.body

    def get_sms_code(self):
        print '\n=========================get_sms_code========================='
        time.sleep(20)
        http_client = tornado.httpclient.HTTPClient()
        m_url = 'http://api.eobzz.com/httpApi.do?action=getVcodeAndReleaseMobile&uid=%s&token=%s&mobile=%s' % (self.username, self.eobzz_token, self.phone_num)
        print m_url
        request = tornado.httpclient.HTTPRequest(url= m_url, method='GET', connect_timeout=20, request_timeout=60)
        response = http_client.fetch(request)
        print response.body
        tmp = response.body.split('|')
        if len(tmp) != 2:
            http_client.close()
            self.get_sms_code()
        else:
            self.sms_code = tmp[1]
            number = re.compile(ur'([一二三四五六七八九零十百千万亿]+|[0-9]+[,]*[0-9]+.[0-9]+)')
            pattern = re.compile(number)
            all_res = pattern.findall(self.sms_code)
            self.sms_code = all_res[0]

    def bind_invite_code(self):
        print '\n=========================bind_invite_code========================='
        send_data = {}
        http_client = tornado.httpclient.HTTPClient()
        send_data['phone'] = self.phone_num
        send_data['code'] = self.sms_code
        _json_data = json.dumps(send_data)
        print _json_data
        http_body = _json_data
        header_data = {}
        header_data['Accept-Language'] = 'zh-Hans-CN;q=1.0'
        header_data['Content-Type'] = 'application/json'

        request = tornado.httpclient.HTTPRequest(url='http://api.api.chongdingdahui.com/user/login', method='POST', connect_timeout=10, request_timeout=60, body=http_body, headers = header_data, user_agent='LiveTrivia/1.0.5 (com.chongdingdahui.app; build:0.2.0; iOS 11.2.2) Alamofire/4.6.0')
        response = http_client.fetch(request)
        print response.body
        _json_res = json.loads(response.body)
        if _json_res['code'] == 0:
            Invite_data = {}
            Invite_data['inviteCode'] = self.invite_code
            _json_invite = json.dumps(Invite_data)
            header_data['X-Live-Session-Token'] = _json_res['data']['user']['sessionToken']
            Invite_request = tornado.httpclient.HTTPRequest(url='http://api.api.chongdingdahui.com/user/bindInviteCode', method='POST', connect_timeout=10, request_timeout=60, body=_json_invite, headers = header_data, user_agent='LiveTrivia/1.0.5 (com.chongdingdahui.app; build:0.2.0; iOS 11.2.2) Alamofire/4.6.0')
            Invite_response = http_client.fetch(Invite_request)
            print Invite_response.body

    def set_ignore_list(self):
        print '\n=========================set_ignore_list========================='
        http_client = tornado.httpclient.HTTPClient()
        m_url = 'http://api.eobzz.com/httpApi.do?action=addIgnoreList&uid=%s&token=%s&mobiles=%s&pid=36848' % (self.username, self.eobzz_token, self.phone_num)
        print m_url
        request = tornado.httpclient.HTTPRequest(url= m_url, method='GET', connect_timeout=20, request_timeout=60)
        response = http_client.fetch(request)
        print response.body

if __name__ == '__main__':
    m_server = Server()
    m_server.get_eobzz_token()
    m_server.get_phone_num()
    m_server.send_sms_code()
    m_server.get_sms_code()
    m_server.bind_invite_code()
    m_server.set_ignore_list()
