-- support uri parameters like: "control?target=graph_duration&step=10m&op=more"
-- target: graph_duration, thread_num, thread_load
-- step: 10m / 1h / 12h (means 10 minutes / 1 hours / 12 hours)
-- op: more / less  (means increace / decreace)

local conf_file = '/usr/local/sandai/redis-c-cluster/rp3m/rp.conf'
local notify_file = '/usr/local/sandai/redis-c-cluster/rp3m/rp.notify'
local home_location = '/rp3m'



local config_limit = {graph_duration = {upper = (12*50400), lower = 600}
                     ,thread_num     = {upper = 48, lower = 0}
                     ,thread_load    = {upper = 10, lower = 0}}
                     
local config_args = {graph_duration = 0
                    ,thread_num = 0
                    ,thread_load = 0}
                    
--config_args["graph_duration"] = 0
--config_args["thread_num"] = 0
--config_args["thread_load"] = 0



function load_config()
    local fp,err = io.open(conf_file, 'r')
    if not fp then
        ngx.log(ngx.INFO, "fail to open config file for read: ".. err)
        return false
    end

    for line in fp:lines() do
        local key,val = line:match("%s*(%S+)%s+(%S+)")
        --ngx.log(ngx.DEBUG, "load_config read:["..key.. "]->["..val.."]")
        if key == "graph_duration" or key == "thread_num" or key == "thread_load" then
            if not val then
                ngx.say('current value of configuration ['.. key .. '] not exist')
                return false
            end
            config_args[key] = tonumber(val)
        end
    end

    fp:close()
    return true
end

function write_config()
    local fp,err = io.open(conf_file, 'w')
    if not fp then
        ngx.say('error in open config file for write: '.. err)
        return false
    end

    for key,val in pairs(config_args) do
        local ret,err = fp:write(key.." "..val.."\n")
        if not ret then
            ngx.say('error in write config file '..conf_file..' : '.. err)
            return false
        end
    end

    fp:close()
    return true
end

function parse_step(unit_str)
    --ngx.log(ngx.DEBUG, "pare_step:"..unit_str)
    if unit_str == "10m" then
        return 60*10
    elseif unit_str == "1h" then
        return 60 * 60
    elseif unit_str == "12h" then
        return 60 * 60 * 12
    else
        return 0
    end
end

if not load_config() then
    ngx.log(ngx.INFO, "config file ".. conf_file .." will be new created")
end

local args = ngx.req.get_uri_args()
local step = 0

local target = args["target"]
--ngx.log(ngx.DEBUG, "target:"..args["target"])

if target and config_args[target] then

    --translate step
    if target == "graph_duration" then
		step = parse_step(args["step"])
    else
        step = args["step"];
    end
    
	if step <= 0 then
		ngx.say('invalid config step ' .. args["step"])
		return
	end

    --update config variable
    if args["op"]  == "more" then
        config_args[target] = config_args[target] + step
    elseif args["op"]  == "less" then
        config_args[target] = config_args[target] - step
    else
        ngx.say('invalid config op ' .. args["op"])
        return
    end

    if config_args[target] > config_limit[target]["upper"] then
        config_args[target] = config_limit[target]["upper"]
    end
    
    if config_args[target] < config_limit[target]["lower"] then
        config_args[target] = config_limit[target]["lower"]
    end
end

--print result

if target and args["op"] then
    ngx.log(ngx.DEBUG, "change " .. target .. " to ".. args["op"] .. " " .. step)
else
    local req_para = ""
    for k,v in pairs(args) do
       req_para = req_para .. k .. ":" .. v.. " ,"
    end
    ngx.log(ngx.DEBUG, "invalid command parameter:" .. req_para)
end

if not write_config() then
    return
end

os.execute('touch '..notify_file)
ngx.sleep(1)
ngx.redirect(home_location)
