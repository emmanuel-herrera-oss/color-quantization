import { parseArgs } from 'node:util'
const validateInput = (values: any) => {
    let error = false
    if(!values.from) {
        error = true
    }
    if(!values.to) {
        error = true
    }
    return error
}
export const getInput = (): InputOptions => {
    const args = process.argv.slice(2);
    const options = {
    from: {
        type: 'string',
        short: 'f'
    },
    to: {
        type: 'string',
        short: 't'
    },
    clusters: {
        type: 'string',
        short: 'k',
        default: '8'
    },
    iter: {
        type: 'string',
        short: 'i',
        default: '1000'
    }
    };
    const { values } = parseArgs({ args, options });
    const isError = validateInput(values)
    if(isError) {
        console.log('Usage: bun run cq.ts -f [fromPng] -t [toPng] -k [cluster_count] -i [max_iterations]')
        process.exit()
    }
    else {
        return {
            from: values.from,
            to: values.to,
            clusters: Number(values.clusters),
            iter: Number(values.iter)
        }
    }
}

export type InputOptions = {
    from: string,
    to: string,
    clusters: number,
    iter: number
}