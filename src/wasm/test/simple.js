const fs = require('fs');

function run() {
  function createWebAssembly(bytes) {
    const memory = new WebAssembly.Memory({ initial: 256, maximum: 256 });
    const env = {
      abortStackOverflow: (err) => { throw new Error(`overflow: ${err}`); },
      table: new WebAssembly.Table({ initial: 0, maximum: 0, element: 'anyfunc' }),
      __table_base: 0,
      memory,
      __memory_base: 1024,
      STACKTOP: 0,
      STACK_MAX: memory.buffer.byteLength,
    };
    return WebAssembly.instantiate(bytes, { env });
  }

  var result;
  fs.readFile('./simple.wasm', (err, data) => {
    if (err) throw new Error(`${err}`);
    result = createWebAssembly(new Uint8Array(data));
    return;
  });
  console.log(JSON.stringify(result));
  console.log(result.instance.exports._fibonacci(9));
}
run();