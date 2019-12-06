<?php

namespace Axiom\Ghost\Statements;

use Axiom\Ghost\Token;
use Axiom\Ghost\Statements\Statement;

class ExpressionStatement extends Statement
{
    /**
	 * @var Expression
	 */
	public $expression;

    /**
     * Create a new Expression statement instance.
     * 
     * param  Expression  $expression
     */
    public function __construct($expression)
    {
        $this->expression = $expression;
    }

    /**
	 * Accept a visitor instance.
	 *
	 * @param  Visitor  $visitor
	 * @return Visitor
	*/
	public function accept($visitor)
	{
		return $visitor->visitExpressionStatement($this);
	}
}