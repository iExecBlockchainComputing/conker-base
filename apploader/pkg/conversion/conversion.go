package conversion

import (
	"encoding/json"
	"strconv"
)

// Interface2String converts interface{} to string
func Interface2String(value interface{}) string {
	if value == nil {
		return ""
	}

	switch v := value.(type) {
	case float64:
		return strconv.FormatFloat(v, 'f', -1, 64)
	case float32:
		return strconv.FormatFloat(float64(v), 'f', -1, 64)
	case int:
		return strconv.Itoa(v)
	case uint:
		return strconv.Itoa(int(v))
	case int8:
		return strconv.Itoa(int(v))
	case uint8:
		return strconv.Itoa(int(v))
	case int16:
		return strconv.Itoa(int(v))
	case uint16:
		return strconv.Itoa(int(v))
	case int32:
		return strconv.Itoa(int(v))
	case uint32:
		return strconv.Itoa(int(v))
	case int64:
		return strconv.FormatInt(v, 10)
	case uint64:
		return strconv.FormatUint(v, 10)
	case string:
		return v
	case []byte:
		return string(v)
	default:
		newValue, _ := json.Marshal(value)
		return string(newValue)
	}
}
